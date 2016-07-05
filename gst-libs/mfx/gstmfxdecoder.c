/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include <mfxplugin.h>
#include <mfxvp8.h>

#include "gstmfxdecoder.h"
#include "gstmfxfilter.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxvideometa.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxtask.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxDecoder
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GstMfxTaskAggregator *aggregator;
  GstMfxTask *decode;
  GstMfxProfile profile;
  GstMfxSurfacePool *pool;
  GstMfxFilter *filter;
  GByteArray *bitstream;

  GQueue *frames;
  guint32 num_decoded_frames;

  mfxSession session;
  mfxVideoParam param;
  mfxBitstream bs;
  mfxPluginUID plugin_uid;
  mfxFrameAllocRequest request;

  GstVideoInfo info;
  gboolean inited;
  gboolean mapped;
};

GstMfxProfile
gst_mfx_decoder_get_profile (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, 0);

  return decoder->profile;
}

static void
gst_mfx_decoder_finalize (GstMfxDecoder * decoder)
{
  gst_mfx_filter_replace (&decoder->filter, NULL);

  g_byte_array_unref (decoder->bitstream);
  gst_mfx_task_aggregator_unref (decoder->aggregator);
  gst_mfx_task_replace (&decoder->decode, NULL);
  gst_mfx_surface_pool_replace (&decoder->pool, NULL);
  g_queue_free(decoder->frames);

  if ((decoder->param.mfx.CodecId == MFX_CODEC_HEVC) ||
      (decoder->param.mfx.CodecId == MFX_CODEC_VP8))
    MFXVideoUSER_UnLoad(decoder->session, &decoder->plugin_uid);

  MFXVideoDECODE_Close (decoder->session);
}

static mfxStatus
gst_mfx_decoder_configure_plugins (GstMfxDecoder * decoder)
{
  mfxStatus sts;
  guint i, c;

  switch (decoder->param.mfx.CodecId) {
    case MFX_CODEC_HEVC:
    {
      gchar *uids[] = {
        "33a61c0b4c27454ca8d85dde757c6f8e",
        "15dd936825ad475ea34e35f3f54217a6",
        NULL
      };
      for (i = 0; uids[i]; i++) {
        for (c = 0; c < sizeof (decoder->plugin_uid.Data); c++)
          sscanf (uids[i] + 2 * c, "%2hhx", decoder->plugin_uid.Data + c);
        sts = MFXVideoUSER_Load (decoder->session, &decoder->plugin_uid, 1);
        if (MFX_ERR_NONE == sts) {
          if (!g_strcmp0 (uids[i], "15dd936825ad475ea34e35f3f54217a6"))
            decoder->param.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
          break;
        }
      }
    }
      break;
    case MFX_CODEC_VP8:
    {
      gchar *uid = "f622394d8d87452f878c51f2fc9b4131";
      for (c = 0; c < sizeof (decoder->plugin_uid.Data); c++)
        sscanf (uid + 2 * c, "%2hhx", decoder->plugin_uid.Data + c);
      sts = MFXVideoUSER_Load (decoder->session, &decoder->plugin_uid, 1);
    }
      break;
    default:
      sts = MFX_ERR_NONE;
  }

  return sts;
}

static void
gst_mfx_decoder_set_video_properties (GstMfxDecoder * decoder)
{
  mfxFrameInfo *frame_info = &decoder->param.mfx.FrameInfo;

  frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  frame_info->FourCC = MFX_FOURCC_NV12;
  frame_info->PicStruct = GST_VIDEO_INFO_IS_INTERLACED (&decoder->info) ?
      (GST_VIDEO_INFO_FLAG_IS_SET (&decoder->info,
          GST_VIDEO_FRAME_FLAG_TFF) ? MFX_PICSTRUCT_FIELD_TFF :
          MFX_PICSTRUCT_FIELD_BFF)
      : MFX_PICSTRUCT_PROGRESSIVE;

  frame_info->CropX = 0;
  frame_info->CropY = 0;
  frame_info->CropW = decoder->info.width;
  frame_info->CropH = decoder->info.height;
  frame_info->FrameRateExtN = decoder->info.fps_n ? decoder->info.fps_n : 30;
  frame_info->FrameRateExtD = decoder->info.fps_d;
  frame_info->AspectRatioW = decoder->info.par_n;
  frame_info->AspectRatioH = decoder->info.par_d;
  frame_info->BitDepthChroma = 8;
  frame_info->BitDepthLuma = 8;

  frame_info->Width = GST_ROUND_UP_16 (decoder->info.width);
  frame_info->Height =
      (MFX_PICSTRUCT_PROGRESSIVE == frame_info->PicStruct) ?
          GST_ROUND_UP_16 (decoder->info.height) :
          GST_ROUND_UP_32 (decoder->info.height);

  decoder->param.mfx.CodecProfile =
      gst_mfx_profile_get_codec_profile(decoder->profile);
}

static gboolean
task_init (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFX_ERR_NONE;

  decoder->decode = gst_mfx_task_new (decoder->aggregator,
      GST_MFX_TASK_DECODER);
  if (!decoder->decode)
    return FALSE;

  gst_mfx_task_aggregator_set_current_task (decoder->aggregator,
      decoder->decode);
  decoder->session = gst_mfx_task_get_session (decoder->decode);

  gst_mfx_decoder_set_video_properties (decoder);

  sts = gst_mfx_decoder_configure_plugins (decoder);
  if (sts < 0)
    return FALSE;

  sts = MFXVideoDECODE_QueryIOSurf (decoder->session, &decoder->param,
      &decoder->request);
  if (sts < 0) {
    GST_ERROR ("Unable to query decode allocation request %d", sts);
    return FALSE;
  } else if (sts > 0) {
    decoder->param.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  }

  gst_mfx_task_set_request (decoder->decode, &decoder->request);

  return TRUE;
}

static gboolean
gst_mfx_decoder_init (GstMfxDecoder * decoder,
    GstMfxTaskAggregator * aggregator, GstMfxProfile profile,
    GstVideoInfo * info, mfxU16 async_depth, gboolean mapped)
{
  decoder->info = *info;
  decoder->profile = profile;
  decoder->param.mfx.CodecId = gst_mfx_profile_get_codec(profile);
  decoder->mapped = mapped;
  decoder->param.AsyncDepth = async_depth;
  decoder->param.IOPattern = mapped ?
      MFX_IOPATTERN_OUT_SYSTEM_MEMORY : MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  decoder->inited = FALSE;
  decoder->bs.MaxLength = 1024 * 16;
  decoder->bitstream = g_byte_array_sized_new (decoder->bs.MaxLength);
  if (!decoder->bitstream)
    return FALSE;
  decoder->frames = g_queue_new();
  if (!decoder->frames)
    return FALSE;

  decoder->aggregator = gst_mfx_task_aggregator_ref (aggregator);
  if (!task_init(decoder))
    return FALSE;

  return TRUE;
}

static inline const GstMfxMiniObjectClass *
gst_mfx_decoder_class (void)
{
  static const GstMfxMiniObjectClass GstMfxDecoderClass = {
    sizeof (GstMfxDecoder),
    (GDestroyNotify) gst_mfx_decoder_finalize
  };
  return &GstMfxDecoderClass;
}

GstMfxDecoder *
gst_mfx_decoder_new (GstMfxTaskAggregator * aggregator,
    GstMfxProfile profile, GstVideoInfo * info,
    mfxU16 async_depth, gboolean mapped)
{
  GstMfxDecoder *decoder;

  g_return_val_if_fail (aggregator != NULL, NULL);

  decoder = gst_mfx_mini_object_new0 (gst_mfx_decoder_class ());
  if (!decoder)
    goto error;

  if (!gst_mfx_decoder_init (decoder, aggregator, profile, info,
      async_depth, mapped))
    goto error;

  return decoder;
error:
  gst_mfx_mini_object_unref (decoder);
  return NULL;
}

GstMfxDecoder *
gst_mfx_decoder_ref (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, NULL);

  return gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (decoder));
}

void
gst_mfx_decoder_unref (GstMfxDecoder * decoder)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (decoder));
}

void
gst_mfx_decoder_replace (GstMfxDecoder ** old_decoder_ptr,
    GstMfxDecoder * new_decoder)
{
  g_return_if_fail (old_decoder_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_decoder_ptr,
      GST_MFX_MINI_OBJECT (new_decoder));
}

static GstMfxDecoderStatus
gst_mfx_decoder_start (GstMfxDecoder * decoder)
{
  GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_SUCCESS;
  GstVideoFormat vformat = GST_VIDEO_INFO_FORMAT (&decoder->info);
  mfxStatus sts = MFX_ERR_NONE;
  gboolean mapped;

  sts = MFXVideoDECODE_DecodeHeader (decoder->session, &decoder->bs,
      &decoder->param);
  if (MFX_ERR_MORE_DATA == sts) {
    return GST_MFX_DECODER_STATUS_ERROR_NO_DATA;
  } else if (sts < 0) {
    GST_ERROR ("Decode header error %d\n", sts);
    return GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER;
  }

  mapped = !!(decoder->param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
  if (!mapped)
    gst_mfx_task_use_video_memory (decoder->decode);

  if (vformat != GST_VIDEO_FORMAT_NV12 || mapped != decoder->mapped) {
    decoder->filter = gst_mfx_filter_new_with_task (decoder->aggregator,
        decoder->decode, GST_MFX_TASK_VPP_IN, mapped, decoder->mapped);

    if (!decoder->filter)
      return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;

    decoder->request.Type =
        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE |
        MFX_MEMTYPE_EXPORT_FRAME;

    decoder->request.NumFrameSuggested += (1 - decoder->param.AsyncDepth);

    gst_mfx_filter_set_request (decoder->filter, &decoder->request,
        GST_MFX_TASK_VPP_IN);

    gst_mfx_filter_set_frame_info (decoder->filter, &decoder->info);

    if (vformat != GST_VIDEO_FORMAT_NV12)
      gst_mfx_filter_set_format (decoder->filter, vformat);

    if (!gst_mfx_filter_prepare (decoder->filter))
      return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;

    decoder->pool = gst_mfx_filter_get_pool (decoder->filter,
        GST_MFX_TASK_VPP_IN);
    if (!decoder->pool)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;
  }

  sts = MFXVideoDECODE_Init (decoder->session, &decoder->param);
  if (sts < 0) {
    GST_ERROR ("Error initializing the MFX video decoder %d", sts);
    return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;
  }

  if (!decoder->pool) {
    decoder->pool = gst_mfx_surface_pool_new_with_task (decoder->decode);
    if (!decoder->pool)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;
  }

  return ret;
}

GstMfxDecoderStatus
gst_mfx_decoder_decode (GstMfxDecoder * decoder,
    GstVideoCodecFrame * frame, GstVideoCodecFrame ** out_frame)
{
  GstMapInfo minfo;
  GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_SUCCESS;
  GstMfxFilterStatus filter_sts;
  GstMfxSurfaceProxy *proxy, *filter_proxy;
  mfxFrameSurface1 *insurf, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;

  g_queue_push_head (decoder->frames, gst_video_codec_frame_ref(frame));

  if (!gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ)) {
    GST_ERROR ("Failed to map input buffer");
    return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
  }

  if (minfo.size) {
    decoder->bs.DataLength += minfo.size;
    if (decoder->bs.MaxLength <
        decoder->bs.DataLength + decoder->bitstream->len)
      decoder->bs.MaxLength = decoder->bs.DataLength + decoder->bitstream->len;
    decoder->bitstream = g_byte_array_append (decoder->bitstream,
        minfo.data, minfo.size);
    decoder->bs.Data = decoder->bitstream->data;
  }

  /* Initialize the MFX decoder session */
  if (G_UNLIKELY (!decoder->inited)) {
    ret = gst_mfx_decoder_start (decoder);
    if (GST_MFX_DECODER_STATUS_SUCCESS == ret)
      decoder->inited = TRUE;
    else
      goto end;
  }

  do {
    proxy = gst_mfx_surface_proxy_new_from_pool (decoder->pool);
    if (!proxy)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

    insurf = gst_mfx_surface_proxy_get_frame_surface (proxy);
    sts = MFXVideoDECODE_DecodeFrameAsync (decoder->session, &decoder->bs,
        insurf, &outsurf, &syncp);

    if (MFX_WRN_DEVICE_BUSY == sts)
      g_usleep (500);
  } while (MFX_WRN_DEVICE_BUSY == sts || MFX_ERR_MORE_SURFACE == sts);

  if (sts != MFX_ERR_NONE &&
      sts != MFX_ERR_MORE_DATA &&
      sts != MFX_WRN_VIDEO_PARAM_CHANGED) {
    GST_ERROR ("Error during MFX decoding.");
    ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
    goto end;
  }

  if (sts == MFX_ERR_MORE_DATA || sts > 0) {
    if (decoder->num_decoded_frames)
      gst_video_codec_frame_unref (g_queue_pop_head (decoder->frames));
    ret = GST_MFX_DECODER_STATUS_ERROR_NO_DATA;
    goto end;
  }

  if (syncp) {
    decoder->bitstream = g_byte_array_remove_range (decoder->bitstream, 0,
        decoder->bs.DataOffset);
    decoder->bs.DataOffset = 0;

    if (!gst_mfx_task_has_type (decoder->decode, GST_MFX_TASK_ENCODER))
      do {
        sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
      } while (MFX_WRN_IN_EXECUTION == sts);

    proxy = gst_mfx_surface_pool_find_proxy (decoder->pool, outsurf);

    if (gst_mfx_task_has_type (decoder->decode, GST_MFX_TASK_VPP_IN)) {
      filter_sts = gst_mfx_filter_process (decoder->filter, proxy,
          &filter_proxy);
      if (GST_MFX_FILTER_STATUS_SUCCESS != filter_sts) {
        GST_ERROR ("MFX post-processing error while decoding.");
        ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
        goto end;
      }
      proxy = filter_proxy;
    }
    *out_frame = g_queue_pop_tail (decoder->frames);
    gst_video_codec_frame_set_user_data(*out_frame,
        gst_mfx_surface_proxy_ref (proxy), gst_mfx_surface_proxy_unref);
    (*out_frame)->pts = (*out_frame)->dts;

    decoder->num_decoded_frames++;
    GST_DEBUG("decoded frame number : %ld\n", decoder->num_decoded_frames);
  }

end:
  gst_buffer_unmap (frame->input_buffer, &minfo);

  return ret;
}

GstMfxDecoderStatus
gst_mfx_decoder_flush (GstMfxDecoder * decoder,
    GstVideoCodecFrame ** out_frame)
{
  GstMfxDecoderStatus ret;
  GstMfxFilterStatus filter_sts;
  GstMfxSurfaceProxy *proxy, *filter_proxy;
  mfxFrameSurface1 *insurf, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts;

  do {
    proxy = gst_mfx_surface_proxy_new_from_pool (decoder->pool);
    if (!proxy)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

    insurf = gst_mfx_surface_proxy_get_frame_surface (proxy);
    sts = MFXVideoDECODE_DecodeFrameAsync (decoder->session, NULL,
        insurf, &outsurf, &syncp);

    if (sts == MFX_WRN_DEVICE_BUSY)
      g_usleep (500);
  } while (MFX_WRN_DEVICE_BUSY == sts);

  if (sts == MFX_ERR_NONE) {
    do {
      sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
    } while (MFX_WRN_IN_EXECUTION == sts);

    proxy = gst_mfx_surface_pool_find_proxy (decoder->pool, outsurf);

    if (gst_mfx_task_has_type (decoder->decode, GST_MFX_TASK_VPP_IN)) {
      filter_sts = gst_mfx_filter_process (decoder->filter, proxy,
          &filter_proxy);

      if (GST_MFX_FILTER_STATUS_SUCCESS != filter_sts) {
        GST_ERROR ("MFX post-processing error while decoding.");
        ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
      }
      proxy = filter_proxy;
    }
    *out_frame = g_queue_pop_tail (decoder->frames);
    gst_video_codec_frame_set_user_data(*out_frame,
        gst_mfx_surface_proxy_ref (proxy), gst_mfx_surface_proxy_unref);
    (*out_frame)->pts = (*out_frame)->dts;

    decoder->num_decoded_frames++;
    GST_DEBUG("decoded frame number : %ld\n", decoder->num_decoded_frames);
    ret = GST_MFX_DECODER_STATUS_SUCCESS;
  } else {
    ret = GST_MFX_DECODER_STATUS_FLUSHED;
  }
  return ret;
}
