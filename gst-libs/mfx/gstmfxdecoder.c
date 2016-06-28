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
#include "gstmfxprofile.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxDecoder
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GstMfxTaskAggregator *aggregator;
  GstMfxTask *decode_task;
  GstMfxSurfacePool *pool;
  GstMfxFilter *filter;
  GByteArray *bitstream;

  GAsyncQueue *frames;

  mfxSession session;
  mfxVideoParam param;
  mfxBitstream bs;
  GstMfxProfile profile;

  GstVideoInfo info;
  gboolean decoder_inited;
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
  gst_mfx_task_replace (&decoder->decode_task, NULL);
  gst_mfx_surface_pool_replace (&decoder->pool, NULL);

  MFXVideoDECODE_Close (decoder->session);
}

static mfxStatus
gst_mfx_decoder_load_decoder_plugins (GstMfxDecoder * decoder, gchar ** out_uid)
{
  mfxPluginUID uid;
  mfxStatus sts;
  guint i, c;

  switch (decoder->param.mfx.CodecId) {
    case MFX_CODEC_HEVC:
    {
      gchar *plugin_uids[] = {
        "33a61c0b4c27454ca8d85dde757c6f8e",
        "15dd936825ad475ea34e35f3f54217a6",
        NULL
      };
      for (i = 0; plugin_uids[i]; i++) {
        for (c = 0; c < sizeof (uid.Data); c++)
          sscanf (plugin_uids[i] + 2 * c, "%2hhx", uid.Data + c);
        sts = MFXVideoUSER_Load (decoder->session, &uid, 1);
        if (MFX_ERR_NONE == sts) {
          *out_uid = g_strdup (plugin_uids[i]);
          break;
        }
      }
    }
      break;
    case MFX_CODEC_VP8:
      *out_uid = g_strdup ("f622394d8d87452f878c51f2fc9b4131");
      for (c = 0; c < sizeof (uid.Data); c++)
        sscanf (*out_uid + 2 * c, "%2hhx", uid.Data + c);
      sts = MFXVideoUSER_Load (decoder->session, &uid, 1);
      break;
    default:
      sts = MFX_ERR_NONE;
  }

  return sts;
}

static gboolean
gst_mfx_decoder_init (GstMfxDecoder * decoder,
    GstMfxTaskAggregator * aggregator, GstMfxProfile profile,
    GstVideoInfo * info, mfxU16 async_depth, gboolean mapped)
{
  mfxStatus sts = MFX_ERR_NONE;
  gchar *uid = NULL;

  decoder->info = *info;
  decoder->profile = profile;
  decoder->param.mfx.CodecId = gst_mfx_profile_get_codec(profile);
  decoder->param.AsyncDepth = async_depth;
  decoder->decoder_inited = FALSE;
  decoder->mapped = mapped;
  decoder->bs.MaxLength = 1024 * 16;
  //decoder->bs.DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;
  decoder->frames = g_async_queue_new();

  decoder->aggregator = gst_mfx_task_aggregator_ref (aggregator);
  decoder->decode_task = gst_mfx_task_new (decoder->aggregator,
      GST_MFX_TASK_DECODER);
  if (!decoder->decode_task)
    return FALSE;

  gst_mfx_task_aggregator_set_current_task (decoder->aggregator,
      decoder->decode_task);
  decoder->session = gst_mfx_task_get_session (decoder->decode_task);

  sts = gst_mfx_decoder_load_decoder_plugins (decoder, &uid);
  if (sts < 0)
    return FALSE;

  if (!g_strcmp0 (uid, "15dd936825ad475ea34e35f3f54217a6")) {
    mapped = TRUE;
    decoder->bs.DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;
  }
  g_free (uid);

  decoder->bitstream = g_byte_array_sized_new (decoder->bs.MaxLength);
  if (!decoder->bitstream)
    return FALSE;
  decoder->param.IOPattern = mapped ?
      MFX_IOPATTERN_OUT_SYSTEM_MEMORY : MFX_IOPATTERN_OUT_VIDEO_MEMORY;

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

static void
gst_mfx_decoder_set_video_properties (GstMfxDecoder * decoder)
{
  mfxFrameInfo *frame_info = &decoder->param.mfx.FrameInfo;

  frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  frame_info->FourCC = MFX_FOURCC_NV12;
  frame_info->PicStruct =
      GST_VIDEO_INFO_IS_INTERLACED (&decoder->info) ? (GST_VIDEO_INFO_FLAG_IS_SET (&decoder->info,
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
      GST_ROUND_UP_16 (decoder->info.height) : GST_ROUND_UP_32 (decoder->info.height);

  decoder->param.mfx.CodecProfile =
      gst_mfx_profile_get_codec_profile(decoder->profile);
}

GstMfxDecoderStatus
gst_mfx_decoder_start (GstMfxDecoder * decoder)
{
  GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_SUCCESS;
  GstVideoFormat vformat = GST_VIDEO_INFO_FORMAT (&decoder->info);
  mfxFrameInfo *frame_info = &decoder->param.mfx.FrameInfo;
  mfxStatus sts = MFX_ERR_NONE;
  mfxFrameAllocRequest dec_request;
  gboolean mapped;

  memset (&dec_request, 0, sizeof (mfxFrameAllocRequest));

  gst_mfx_decoder_set_video_properties (decoder);

  if (!frame_info->FrameRateExtN)
    frame_info->FrameRateExtN = decoder->info.fps_n ? decoder->info.fps_n : 30;
  if (!frame_info->FrameRateExtD)
    frame_info->FrameRateExtD = decoder->info.fps_d;
  if (!frame_info->AspectRatioW)
    frame_info->AspectRatioW = decoder->info.par_n;
  if (!frame_info->AspectRatioH)
    frame_info->AspectRatioH = decoder->info.par_d;

  sts = MFXVideoDECODE_QueryIOSurf (decoder->session, &decoder->param,
      &dec_request);
  if (sts < 0) {
    GST_ERROR ("Unable to query decode allocation request %d", sts);
    return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;
  } else if (sts > 0) {
    decoder->param.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  }

  mapped = !!(decoder->param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
  if (!mapped)
    gst_mfx_task_use_video_memory (decoder->decode_task);

  dec_request.NumFrameSuggested += (1 - decoder->param.AsyncDepth);

  gst_mfx_task_set_request (decoder->decode_task, &dec_request);

  if (vformat != GST_VIDEO_FORMAT_NV12 || mapped != decoder->mapped) {
    decoder->filter = gst_mfx_filter_new_with_task (decoder->aggregator,
        decoder->decode_task, GST_MFX_TASK_VPP_IN, mapped, decoder->mapped);

    if (!decoder->filter)
      return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;

    dec_request.Type =
        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE |
        MFX_MEMTYPE_EXPORT_FRAME;

    gst_mfx_filter_set_request (decoder->filter, &dec_request,
        GST_MFX_TASK_VPP_IN);

    gst_mfx_filter_set_frame_info (decoder->filter, &decoder->info);

    if (vformat != GST_VIDEO_FORMAT_NV12)
      gst_mfx_filter_set_format (decoder->filter, vformat);

    if (!gst_mfx_filter_start (decoder->filter))
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
    decoder->pool = gst_mfx_surface_pool_new_with_task (decoder->decode_task);
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

  g_async_queue_push(decoder->frames, frame);

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

  if (sts == MFX_ERR_MORE_DATA || sts > 0) {
    ret = GST_MFX_DECODER_STATUS_ERROR_NO_DATA;
    goto end;
  }

  if (sts != MFX_ERR_NONE &&
      sts != MFX_ERR_MORE_DATA && sts != MFX_WRN_VIDEO_PARAM_CHANGED) {
    GST_ERROR ("Error during MFX decoding.");
    ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
    goto end;
  }

  if (syncp) {
    decoder->bitstream = g_byte_array_remove_range (decoder->bitstream, 0,
        decoder->bs.DataOffset);
    decoder->bs.DataOffset = 0;

    do {
      sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
    } while (MFX_WRN_IN_EXECUTION == sts);

    proxy = gst_mfx_surface_pool_find_proxy (decoder->pool, outsurf);

    if (gst_mfx_task_has_type (decoder->decode_task, GST_MFX_TASK_VPP_IN)) {
      filter_sts = gst_mfx_filter_process (decoder->filter, proxy,
          &filter_proxy);
      if (GST_MFX_FILTER_STATUS_SUCCESS != filter_sts) {
        GST_ERROR ("MFX post-processing error while decoding.");
        ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
        goto end;
      }
      proxy = filter_proxy;
    }

    *out_frame = g_async_queue_pop(decoder->frames);
    gst_video_codec_frame_set_user_data(*out_frame, proxy, NULL);
  }

end:
  gst_buffer_unmap (frame->input_buffer, &minfo);

  return ret;
}
