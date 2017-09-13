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
#include "gstmfxsurface.h"
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
  GByteArray *codec_data;

  GQueue decoded_frames;
  GQueue pending_frames;
  GQueue discarded_frames;

  mfxSession session;
  mfxVideoParam params;
  mfxFrameAllocRequest request;
  mfxBitstream bs;
  mfxPluginUID plugin_uid;

  GstVideoInfo info;
  gboolean inited;
  gboolean was_reset;
  gboolean has_ready_frames;
  gboolean memtype_is_system;
  gboolean enable_csc;
  gboolean enable_deinterlace;
  gboolean skip_corrupted_frames;
  gboolean can_double_deinterlace;
  gboolean is_avc;
  guint num_partial_frames;

  /* For special double frame rate deinterlacing case */
  GstClockTime current_pts;
  GstClockTime duration;
  GstClockTime pts_offset;
};

GstMfxProfile
gst_mfx_decoder_get_profile (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, 0);

  return decoder->profile;
}

gboolean
gst_mfx_decoder_get_decoded_frames (GstMfxDecoder * decoder,
    GstVideoCodecFrame ** out_frame)
{
  g_return_val_if_fail (decoder != NULL, FALSE);

  *out_frame = g_queue_pop_tail (&decoder->decoded_frames);
  return *out_frame != NULL;
}

GstVideoCodecFrame *
gst_mfx_decoder_get_discarded_frame (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, NULL);

  return g_queue_pop_tail (&decoder->discarded_frames);
}

GstVideoInfo *
gst_mfx_decoder_get_video_info (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, NULL);

  return &decoder->info;
}

void
gst_mfx_decoder_skip_corrupted_frames (GstMfxDecoder * decoder)
{
  g_return_if_fail (decoder != NULL);

  decoder->skip_corrupted_frames = TRUE;
}

void
gst_mfx_decoder_should_use_video_memory (GstMfxDecoder * decoder,
    gboolean memtype_is_video)
{
  mfxVideoParam *params;

  g_return_if_fail (decoder != NULL);

  /* The decoder may be forced to use system memory by a following peer
   * MFX VPP task, or due to decoder limitations for that particular
   * codec. In that case, return to confirm the use of system memory */
  params = gst_mfx_task_get_video_params (decoder->decode);

  if (!params) {
    GST_WARNING ("Unable to retrieve task parameters for decoder");
    return;
  }

  if (params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) {
    decoder->memtype_is_system = TRUE;
    gst_mfx_task_ensure_memtype_is_system (decoder->decode);
    return;
  }

  if (memtype_is_video) {
    decoder->params.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  }
  else {
    decoder->memtype_is_system = TRUE;
    decoder->params.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    gst_mfx_task_ensure_memtype_is_system (decoder->decode);
  }

  if (gst_mfx_task_get_task_type (decoder->decode) == GST_MFX_TASK_DECODER)
    params->IOPattern = decoder->params.IOPattern;

  return;
}

static gboolean
init_decoder (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFXVideoDECODE_Init (decoder->session, &decoder->params);
  if (sts < 0) {
    GST_ERROR ("Error re-initializing the MFX video decoder %d", sts);
    return FALSE;
  }

  if (!decoder->pool) {
    decoder->pool = gst_mfx_surface_pool_new_with_task (decoder->decode);
    if (!decoder->pool)
      return FALSE;
  }
  return TRUE;
}

static void
close_decoder (GstMfxDecoder * decoder)
{
  gst_mfx_surface_pool_replace (&decoder->pool, NULL);

  MFXVideoDECODE_Close (decoder->session);
}

static void
gst_mfx_decoder_finalize (GstMfxDecoder * decoder)
{
  gst_mfx_filter_replace (&decoder->filter, NULL);

  g_byte_array_unref (decoder->bitstream);
  if (decoder->codec_data)
    g_byte_array_unref (decoder->codec_data);
  gst_mfx_task_aggregator_unref (decoder->aggregator);

  g_queue_foreach (&decoder->pending_frames,
      (GFunc) gst_video_codec_frame_unref, NULL);
  g_queue_foreach (&decoder->decoded_frames,
      (GFunc) gst_video_codec_frame_unref, NULL);
  g_queue_clear (&decoder->pending_frames);
  g_queue_clear (&decoder->decoded_frames);
  g_queue_clear (&decoder->discarded_frames);

  if ((decoder->params.mfx.CodecId == MFX_CODEC_VP8)
#ifdef USE_VP9_DECODER
      || (decoder->params.mfx.CodecId == MFX_CODEC_VP9)
#endif
      || (decoder->params.mfx.CodecId == MFX_CODEC_HEVC))
    MFXVideoUSER_UnLoad(decoder->session, &decoder->plugin_uid);

  close_decoder (decoder);

  gst_mfx_task_replace (&decoder->decode, NULL);
}

static mfxStatus
gst_mfx_decoder_configure_plugins (GstMfxDecoder * decoder)
{
  mfxStatus sts;

  switch (decoder->params.mfx.CodecId) {
    case MFX_CODEC_HEVC: {
      guint i = 0, c;
      gchar *uids[] = {
        "33a61c0b4c27454ca8d85dde757c6f8e", /* HW decoder */
        "15dd936825ad475ea34e35f3f54217a6", /* SW decoder */
        NULL
      };
      /* HEVC main10 profiles can only be decoded through SW decoder */
      if (decoder->profile == GST_MFX_PROFILE_HEVC_MAIN10)
        i = 1;
      for (; uids[i]; i++) {
        for (c = 0; c < sizeof (decoder->plugin_uid.Data); c++)
          sscanf (uids[i] + 2 * c, "%2hhx", decoder->plugin_uid.Data + c);
        sts = MFXVideoUSER_Load (decoder->session, &decoder->plugin_uid, 1);
        if (MFX_ERR_NONE == sts) {
          if (!g_strcmp0 (uids[i], "15dd936825ad475ea34e35f3f54217a6"))
            decoder->params.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
          break;
        }
      }
      break;
    }
    case MFX_CODEC_VP8:
      decoder->plugin_uid = MFX_PLUGINID_VP8D_HW;
      sts = MFXVideoUSER_Load (decoder->session, &decoder->plugin_uid, 1);

      break;
#ifdef USE_VP9_DECODER
    case MFX_CODEC_VP9:
      decoder->plugin_uid = MFX_PLUGINID_VP9D_HW;
      sts = MFXVideoUSER_Load (decoder->session, &decoder->plugin_uid, 1);

      break;
#endif
    default:
      sts = MFX_ERR_NONE;
  }

  return sts;
}

static void
gst_mfx_decoder_set_video_properties (GstMfxDecoder * decoder)
{
  mfxFrameInfo *frame_info = &decoder->params.mfx.FrameInfo;

  frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  if (decoder->profile == GST_MFX_PROFILE_HEVC_MAIN10)
    frame_info->FourCC = MFX_FOURCC_P010;
  else
    frame_info->FourCC = MFX_FOURCC_NV12;
#if MSDK_CHECK_VERSION(1,19)
  if (decoder->params.mfx.CodecId == MFX_CODEC_JPEG) {
    frame_info->FourCC = MFX_FOURCC_RGB4;
    frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
  }
#endif

  frame_info->PicStruct = GST_VIDEO_INFO_IS_INTERLACED (&decoder->info) ?
      (GST_VIDEO_INFO_FLAG_IS_SET (&decoder->info,
          GST_VIDEO_FRAME_FLAG_TFF) ? MFX_PICSTRUCT_FIELD_TFF :
          MFX_PICSTRUCT_FIELD_BFF)
      : MFX_PICSTRUCT_PROGRESSIVE;

  frame_info->CropX = 0;
  frame_info->CropY = 0;
  frame_info->CropW = decoder->info.width;
  frame_info->CropH = decoder->info.height;
  frame_info->FrameRateExtN = decoder->info.fps_n;
  frame_info->FrameRateExtD = decoder->info.fps_d;
  frame_info->AspectRatioW = decoder->info.par_n;
  frame_info->AspectRatioH = decoder->info.par_d;
  frame_info->BitDepthChroma = 8;
  frame_info->BitDepthLuma = 8;

  frame_info->Width = GST_ROUND_UP_16 (decoder->info.width);
  if (decoder->params.mfx.CodecId == MFX_CODEC_HEVC) {
    frame_info->Height = GST_ROUND_UP_32 (decoder->info.height);
  } else {
    frame_info->Height =
         (MFX_PICSTRUCT_PROGRESSIVE == frame_info->PicStruct) ?
              GST_ROUND_UP_16 (decoder->info.height) :
              GST_ROUND_UP_32 (decoder->info.height);
  }

  decoder->params.mfx.CodecProfile =
      gst_mfx_profile_get_codec_profile(decoder->profile);
}

static gboolean
task_init (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFX_ERR_NONE;
  mfxU32 output_fourcc, decoded_fourcc;

  decoder->decode = gst_mfx_task_new (decoder->aggregator,
      GST_MFX_TASK_DECODER);
  if (!decoder->decode)
    return FALSE;

  gst_mfx_task_aggregator_set_current_task (decoder->aggregator,
      decoder->decode);
  decoder->session = gst_mfx_task_get_session (decoder->decode);

  gst_mfx_decoder_set_video_properties (decoder);

  sts = gst_mfx_decoder_configure_plugins (decoder);
  if (sts < 0) {
    GST_ERROR ("Unable to load plugin %d", sts);
    goto error_load_plugin;
  }

  sts = MFXVideoDECODE_QueryIOSurf (decoder->session, &decoder->params,
          &decoder->request);
  if (sts < 0) {
    GST_ERROR ("Unable to query decode allocation request %d", sts);
    goto error_query_request;
  } else if (sts == MFX_WRN_PARTIAL_ACCELERATION) {
    decoder->params.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  }

  decoder->memtype_is_system =
    !!(decoder->params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
  decoder->request.Type = decoder->memtype_is_system ?
      MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

  if (decoder->memtype_is_system)
    gst_mfx_task_ensure_memtype_is_system (decoder->decode);

  gst_mfx_task_set_request (decoder->decode, &decoder->request);
  gst_mfx_task_set_video_params (decoder->decode, &decoder->params);

  output_fourcc =
      gst_video_format_to_mfx_fourcc (GST_VIDEO_INFO_FORMAT (&decoder->info));
  decoded_fourcc = decoder->params.mfx.FrameInfo.FourCC;

  if (output_fourcc != decoded_fourcc)
    decoder->enable_csc = TRUE;

  return TRUE;

error_load_plugin:
error_query_request:
  {
    gst_mfx_task_unref (decoder->decode);
    return FALSE;
  }
}

static void
gst_mfx_decoder_convert_avc_stream (GstMfxDecoder * decoder, guint8 * cdata,
    gint size)
{
  guint8 startcode[4] = {0, 0, 0, 1};

  if (!decoder || !cdata || !size)
    return;

  gint32 offset = 0;
  gint32 packet_size = 0;

  while (offset < size) {
    packet_size = GST_READ_UINT32_BE(&cdata[offset]);

    offset += 4;
    if (offset + packet_size > size) break;

    decoder->bitstream = g_byte_array_append (decoder->bitstream,
        startcode, 4);
    decoder->bitstream = g_byte_array_append (decoder->bitstream,
        &cdata[offset], packet_size);

    offset += packet_size;
  }

  if (offset != size) {
    GST_ERROR ("AVC stream error, size %d, processed offset %d.",
        size, offset);
  }
}

static gboolean
gst_mfx_decoder_get_codec_data (GstMfxDecoder * decoder, GstBuffer * codec_data)
{
  gboolean res = FALSE;
  if (decoder->params.mfx.CodecId != MFX_CODEC_AVC || !decoder->is_avc ||
      !codec_data) {
    return res;
  }

  GstMapInfo minfo;

  gst_buffer_map(codec_data, &minfo, GST_MAP_READ);

  if (minfo.size) {
    decoder->codec_data = g_byte_array_sized_new (minfo.size);

    guint8 startcode[4] = {0, 0, 0, 1}, nals_cnt = 0, i = 0, j = 0;
    guint8 profile = ((decoder->params.mfx.CodecId ^ decoder->profile) & 0xff);
    guint8 *cdata = minfo.data;
    guint16 nal_size = 0, offset = 0;
    gchar *msgs[2] = { "SPS", "PPS" };

    if (minfo.size < 8) {
      GST_ERROR ("Codec data not enought information.\n");
      goto error;
    }

    if ((cdata[0] != 1) && (cdata[2] != profile)) {
      GST_ERROR ("Codec data header error.\n");
      goto error;
    }

    offset = 5;

    for (i = 0; i < 2; ++i ) {
      if (offset >= minfo.size) {
        GST_ERROR ("Codec data does not contain %s packets.\n", msgs[i]);
        goto error;
      }

      nals_cnt = cdata[offset] & 0x1f;
      ++offset;
      for (j = 0; j < nals_cnt; ++j ) {
        nal_size = GST_READ_UINT16_BE(&cdata[offset]);
        offset += 2;
        if (offset + nal_size > minfo.size) {
          GST_ERROR ("Codec data %s broken.\n", msgs[i]);
          goto error;
        }

        // Only need first set of SPS/PPS.
        if (j < 1) {
          decoder->codec_data = g_byte_array_append (decoder->codec_data,
              startcode, 4);
          decoder->codec_data = g_byte_array_append (decoder->codec_data,
              &cdata[offset], nal_size);
	}
        offset += nal_size;
      }
    }

    if (offset != minfo.size) {
      GST_WARNING ("Codec data mismatch, size %d, processed offset %d.",
          minfo.size, offset);
    }

    res = TRUE;
  }

error:
  gst_buffer_unmap(codec_data, &minfo);

  return res;
}

static gboolean
gst_mfx_decoder_init (GstMfxDecoder * decoder,
    GstMfxTaskAggregator * aggregator, GstMfxProfile profile,
    const GstVideoInfo * info, mfxU16 async_depth, gboolean live_mode,
    gboolean is_avc, GstBuffer * codec_data)
{
  decoder->profile = profile;
  decoder->info = *info;
  if (!decoder->info.fps_n)
    decoder->info.fps_n = 30;
  decoder->duration =
      (decoder->info.fps_d / (gdouble)decoder->info.fps_n) * 1000000000;

  decoder->params.mfx.CodecId = gst_mfx_profile_get_codec(profile);
  decoder->params.AsyncDepth = live_mode ? 1 : async_depth;
  if (live_mode) {
    decoder->bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    /* This is a special fix for Android Auto / Apple Carplay issues */
    if (decoder->params.mfx.CodecId == MFX_CODEC_AVC)
      decoder->params.mfx.DecodedOrder = 1;
  }

  decoder->params.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  decoder->inited = FALSE;
  decoder->bs.MaxLength = 1024 * 16;
  decoder->bitstream = g_byte_array_sized_new (decoder->bs.MaxLength);
  if (!decoder->bitstream)
    return FALSE;

  decoder->is_avc = is_avc;
  if (is_avc && !gst_mfx_decoder_get_codec_data(decoder, codec_data))
    goto error_init;

  decoder->pts_offset = GST_CLOCK_TIME_NONE;

  g_queue_init (&decoder->decoded_frames);
  g_queue_init (&decoder->pending_frames);
  g_queue_init (&decoder->discarded_frames);

  decoder->aggregator = gst_mfx_task_aggregator_ref (aggregator);
  if (!task_init(decoder))
    goto error_init;

  return TRUE;

error_init:
  {
    if (decoder->codec_data)
      g_byte_array_unref (decoder->codec_data);
    g_byte_array_unref (decoder->bitstream);
    return FALSE;
  }
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
    GstMfxProfile profile, const GstVideoInfo * info, mfxU16 async_depth,
    gboolean live_mode, gboolean is_avc, GstBuffer * codec_data)
{
  GstMfxDecoder *decoder;

  g_return_val_if_fail (aggregator != NULL, NULL);

  decoder = gst_mfx_mini_object_new0 (gst_mfx_decoder_class ());
  if (!decoder)
    goto error;

  if (!gst_mfx_decoder_init (decoder, aggregator, profile, info,
            async_depth, live_mode, is_avc, codec_data))
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
  mfxStatus sts = MFX_ERR_NONE;

  /* Retrieve sequence header / layer data for MPEG2 and VC1 */
  if (decoder->params.mfx.CodecId == MFX_CODEC_VC1
      || decoder->params.mfx.CodecId == MFX_CODEC_MPEG2) {
    mfxVideoParam params = decoder->params;
    guint8 sps_data[128];

    mfxExtCodingOptionSPSPPS extradata = {
      .Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS,
      .Header.BufferSz = sizeof (extradata),
      .SPSBuffer = sps_data,.SPSBufSize = sizeof (sps_data)
    };

    mfxExtBuffer *ext_buffers[] = {
      (mfxExtBuffer *) & extradata,
    };

    params.ExtParam = ext_buffers;
    params.NumExtParam = 1;

    sts = MFXVideoDECODE_DecodeHeader (decoder->session, &decoder->bs, &params);
    if (MFX_ERR_MORE_DATA == sts) {
      return GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
    } else if (sts < 0) {
      GST_ERROR ("Decode header error %d\n", sts);
      return GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER;
    }

    if (extradata.SPSBufSize) {
      decoder->codec_data = g_byte_array_sized_new (extradata.SPSBufSize);
      decoder->codec_data = g_byte_array_append (decoder->codec_data,
          sps_data, extradata.SPSBufSize);
    }
  }

  /* Get updated video params if modified by peer MFX element*/
  gst_mfx_task_update_video_params (decoder->decode, &decoder->params);

  if (decoder->params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
    decoder->memtype_is_system = FALSE;
    gst_mfx_task_use_video_memory (decoder->decode);
  }

  if (!init_decoder(decoder))
    return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;

  GST_INFO ("Initialized MFX decoder task using %s memory",
    decoder->memtype_is_system ? "system" : "video");

  return ret;
}

static gboolean
init_filter (GstMfxDecoder * decoder)
{
  mfxU32 output_fourcc =
      gst_video_format_to_mfx_fourcc (GST_VIDEO_INFO_FORMAT (&decoder->info));

  decoder->filter = gst_mfx_filter_new_with_task (decoder->aggregator,
    decoder->decode, GST_MFX_TASK_VPP_IN,
    decoder->memtype_is_system, decoder->memtype_is_system);
  if (!decoder->filter) {
    GST_ERROR ("Unable to initialize filter.");
    return FALSE;
  }

  decoder->request.Type |=
        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE |
        MFX_MEMTYPE_EXPORT_FRAME;

  decoder->request.NumFrameSuggested += (1 - decoder->params.AsyncDepth);

  gst_mfx_filter_set_request (decoder->filter, &decoder->request,
      GST_MFX_TASK_VPP_IN);

  gst_mfx_filter_set_frame_info (decoder->filter,
     &decoder->params.mfx.FrameInfo);
  if (decoder->enable_csc)
    gst_mfx_filter_set_format (decoder->filter, output_fourcc);
  if (decoder->enable_deinterlace) {
    GstMfxDeinterlaceMode di_mode =
      decoder->can_double_deinterlace ? GST_MFX_DEINTERLACE_MODE_ADVANCED_NOREF
        : GST_MFX_DEINTERLACE_MODE_ADVANCED;
    gst_mfx_filter_set_deinterlace_mode (decoder->filter, di_mode);
  }
  gst_mfx_filter_set_async_depth (decoder->filter, decoder->params.AsyncDepth);

  if (!gst_mfx_filter_prepare (decoder->filter)) {
    GST_ERROR ("Unable to set up postprocessing filter.");
    goto error;
  }

  decoder->pool = gst_mfx_filter_get_pool (decoder->filter,
        GST_MFX_TASK_VPP_IN);
  if (!decoder->pool)
    goto error;

  return TRUE;

error:
  gst_mfx_filter_unref (decoder->filter);
  return FALSE;
}

static gboolean
gst_mfx_decoder_reinit (GstMfxDecoder * decoder, mfxFrameInfo * info)
{
  /*
   * Only CSC and deinterlacing didn't involve, it will be re-use back
   * VASurfaces when restart back MSDK decoder.
   */
  if (!(decoder->enable_csc || decoder->enable_deinterlace) && decoder->decode) {
    gst_mfx_task_set_soft_reinit(decoder->decode, TRUE);
  }

  close_decoder(decoder);

  if (info)
    decoder->params.mfx.FrameInfo = *info;

  /* Only initialize filter when need to use CSC or deinterlacing. */
  if ((decoder->enable_csc || decoder->enable_deinterlace) && !decoder->filter)
    if (!init_filter (decoder))
      return FALSE;

  if (!init_decoder (decoder))
    goto error;

  decoder->bs.DataLength += decoder->bs.DataOffset;
  decoder->bs.DataOffset = 0;

  GstMfxSurface *surface;
  mfxFrameSurface1 *insurf, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;

  do {
    surface = gst_mfx_surface_new_from_pool (decoder->pool);
    if (!surface)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

    insurf = gst_mfx_surface_get_frame_surface (surface);
    sts = MFXVideoDECODE_DecodeFrameAsync (decoder->session, &decoder->bs,
	    insurf, &outsurf, &syncp);
    GST_DEBUG ("MFXVideoDECODE_DecodeFrameAsync status: %d", sts);

    if (MFX_WRN_DEVICE_BUSY == sts)
      g_usleep (100);
  } while (sts > 0 || MFX_ERR_MORE_SURFACE == sts);

  if (syncp) {
    do {
      sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 100);
      GST_DEBUG ("MFXVideoCORE_SyncOperation status: %d", sts);
    } while (MFX_WRN_IN_EXECUTION == sts);
  }
  return TRUE;

error:
  gst_mfx_task_set_soft_reinit(decoder->decode, FALSE);
  gst_mfx_surface_pool_replace (&decoder->pool, NULL);
  gst_mfx_filter_replace (&decoder->filter, NULL);
  return FALSE;
}

void
gst_mfx_decoder_reset (GstMfxDecoder * decoder)
{
  if (decoder->info.interlace_mode == GST_VIDEO_INTERLACE_MODE_MIXED
      && decoder->params.mfx.CodecId == MFX_CODEC_AVC)
    return;

  /* Flush pending frames */
  while (!g_queue_is_empty(&decoder->pending_frames))
    g_queue_push_head(&decoder->discarded_frames,
      g_queue_pop_head(&decoder->pending_frames));

  decoder->pts_offset = GST_CLOCK_TIME_NONE;
  decoder->current_pts = 0;

  if (decoder->bitstream->len)
    g_byte_array_remove_range (decoder->bitstream, 0,
      decoder->bitstream->len);
  memset(&decoder->bs, 0, sizeof(mfxBitstream));

  decoder->was_reset = TRUE;
  decoder->has_ready_frames = FALSE;
  decoder->num_partial_frames = 0;

  MFXVideoDECODE_Reset (decoder->session, &decoder->params);
}

static GstVideoCodecFrame *
new_frame (GstMfxDecoder * decoder)
{
  GstVideoCodecFrame *frame = g_slice_new0 (GstVideoCodecFrame);
  if (!frame)
    return NULL;
  frame->ref_count = 1;

  frame->duration = decoder->duration;
  frame->pts = decoder->current_pts + decoder->pts_offset;
  decoder->current_pts += decoder->duration;

  return frame;
}

static void
queue_output_frame (GstMfxDecoder * decoder, GstMfxSurface * surface)
{
  GstVideoCodecFrame *out_frame;

  if (!decoder->can_double_deinterlace)
    out_frame = g_queue_pop_tail (&decoder->pending_frames);
  else
    out_frame = new_frame (decoder);

  gst_video_codec_frame_set_user_data(out_frame,
      gst_mfx_surface_ref (surface), gst_mfx_surface_unref);
  g_queue_push_head(&decoder->decoded_frames, out_frame);

  GST_LOG ("decoded frame : %ld",
    GST_MFX_SURFACE_FRAME_SURFACE (surface)->Data.FrameOrder);
}

static gint
sort_pts (gconstpointer frame1, gconstpointer frame2, gpointer data)
{
  GstClockTime pts1 = ((GstVideoCodecFrame *) (frame1))->pts;
  GstClockTime pts2 = ((GstVideoCodecFrame *) (frame2))->pts;

  return (pts1 > pts2 ? -1 : pts1 == pts2 ? 0 : +1);
}

GstMfxDecoderStatus
gst_mfx_decoder_decode (GstMfxDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstMapInfo minfo;
  GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_SUCCESS;
  GstMfxFilterStatus filter_sts;
  GstMfxSurface *surface, *filter_surface;
  mfxFrameSurface1 *insurf, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;

  if (!GST_CLOCK_TIME_IS_VALID(decoder->pts_offset)
      && GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)
      && GST_CLOCK_TIME_IS_VALID (frame->pts))
    decoder->pts_offset = frame->pts;

  if (!gst_buffer_map (frame->input_buffer, &minfo, GST_MAP_READ)) {
    GST_ERROR ("Failed to map input buffer");
    return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
  }

  if (decoder->was_reset) {
    if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)) {
      /* Sequence header check for I-frames after MPEG2 video seeking */
      if (MFX_CODEC_MPEG2 == decoder->params.mfx.CodecId) {
        decoder->bs.MaxLength = decoder->bs.DataLength = minfo.size;
        decoder->bs.Data = minfo.data;

        sts = MFXVideoDECODE_DecodeHeader (decoder->session, &decoder->bs,
                &decoder->params);
        memset (&decoder->bs, 0, sizeof (mfxBitstream));
        if (MFX_ERR_MORE_DATA == sts) {
          decoder->bitstream = g_byte_array_append (decoder->bitstream,
            decoder->codec_data->data, decoder->codec_data->len);
          decoder->bs.DataLength = decoder->codec_data->len;
          decoder->bs.MaxLength = decoder->bs.DataLength;
          decoder->bs.Data = decoder->bitstream->data;
        }
      }
      decoder->was_reset = FALSE;
    }
    else {
      g_queue_push_head(&decoder->discarded_frames, frame);
      ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
      goto end;
    }
  }

  if (!decoder->can_double_deinterlace) {
    /* Save frames for later synchronization with decoded MFX surfaces */
    g_queue_insert_sorted (&decoder->pending_frames, frame, sort_pts, NULL);
  }
  else {
    g_queue_push_head(&decoder->discarded_frames, frame);
  }

  if (minfo.size) {
    if ((decoder->params.mfx.CodecId == MFX_CODEC_AVC) && decoder->is_avc) {
      if (G_UNLIKELY (!decoder->inited)) {
        decoder->bitstream = g_byte_array_append (decoder->bitstream,
            decoder->codec_data->data, decoder->codec_data->len);
        decoder->bs.DataLength = decoder->codec_data->len;
        decoder->bs.MaxLength = decoder->bitstream->len;
        decoder->bs.Data = decoder->bitstream->data;
      }

      gst_mfx_decoder_convert_avc_stream (decoder, minfo.data, minfo.size);
      decoder->bs.DataLength += minfo.size;
      decoder->bs.MaxLength = decoder->bitstream->len;
      decoder->bs.Data = decoder->bitstream->data;
    } else {
      decoder->bitstream = g_byte_array_append (decoder->bitstream,
          minfo.data, minfo.size);
      decoder->bs.DataLength += minfo.size;
      decoder->bs.MaxLength = decoder->bitstream->len;
      decoder->bs.Data = decoder->bitstream->data;
    }
  }

  if (G_UNLIKELY (!decoder->inited)) {
    ret = gst_mfx_decoder_start (decoder);
    if (GST_MFX_DECODER_STATUS_SUCCESS == ret)
      decoder->inited = TRUE;
    else
      goto end;
  }

  do {
    surface = gst_mfx_surface_new_from_pool (decoder->pool);
    if (!surface)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

    insurf = gst_mfx_surface_get_frame_surface (surface);
    sts = MFXVideoDECODE_DecodeFrameAsync (decoder->session, &decoder->bs,
        insurf, &outsurf, &syncp);
    GST_DEBUG ("MFXVideoDECODE_DecodeFrameAsync status: %d", sts);

    if (MFX_WRN_DEVICE_BUSY == sts)
      g_usleep (100);
  } while (sts > 0 || MFX_ERR_MORE_SURFACE == sts);

  if (MFX_ERR_MORE_DATA == sts) {
    if (decoder->has_ready_frames && !decoder->can_double_deinterlace)
      decoder->num_partial_frames++;
    ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
    goto end;
  }

  if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts) {
    if (!gst_mfx_decoder_reinit(decoder, &insurf->Info)) {
      ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
      goto end;
    }
    ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
    goto end;
  }

  if (MFX_ERR_NONE != sts && MFX_ERR_MORE_DATA != sts) {
    GST_ERROR ("Status %d : Error during MFX decoding", sts);
    ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
    goto end;
  }

  if (syncp) {
    if (decoder->num_partial_frames) {
      GstVideoCodecFrame *cur_frame;
      guint n = g_queue_get_length (&decoder->pending_frames) - 1;

      /* Discard partial frames */
      while (decoder->num_partial_frames
          && (cur_frame = g_queue_peek_nth (&decoder->pending_frames, n))) {
        if ((cur_frame->pts - decoder->pts_offset) % decoder->duration) {
          g_queue_push_head(&decoder->discarded_frames,
            g_queue_pop_nth (&decoder->pending_frames, n));
          decoder->num_partial_frames--;
        }
        n--;
      }
    }

    if (decoder->skip_corrupted_frames
        && insurf->Data.Corrupted & MFX_CORRUPTION_MAJOR) {
      gst_mfx_decoder_reset (decoder);
      ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
      goto end;
    }
    decoder->has_ready_frames = TRUE;

    if (!gst_mfx_task_has_type (decoder->decode, GST_MFX_TASK_ENCODER))
      do {
        sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
        GST_DEBUG ("MFXVideoCORE_SyncOperation status: %d", sts);
      } while (MFX_WRN_IN_EXECUTION == sts);

    surface = gst_mfx_surface_pool_find_surface (decoder->pool, outsurf);

    /* Update stream properties if they have interlaced frames. An interlaced H264
     * can only be detected after decoding the first frame, hence the delayed VPP
     * initialization */
    switch (outsurf->Info.PicStruct) {
      case MFX_PICSTRUCT_PROGRESSIVE:
        if (decoder->info.interlace_mode != GST_VIDEO_INTERLACE_MODE_MIXED)
          decoder->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
        break;
      case MFX_PICSTRUCT_FIELD_TFF:
        GST_VIDEO_INFO_FLAG_SET (&decoder->info, GST_VIDEO_FRAME_FLAG_TFF);
        goto update;
      case MFX_PICSTRUCT_FIELD_BFF:{
        GST_VIDEO_INFO_FLAG_UNSET (&decoder->info, GST_VIDEO_FRAME_FLAG_TFF);
update:
        /* Check if stream has progressive frames first.
         * If it does then it should be a mixed interlaced stream */
        if (decoder->info.interlace_mode == GST_VIDEO_INTERLACE_MODE_PROGRESSIVE
            && decoder->has_ready_frames)
          decoder->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
        else {
          if (decoder->info.interlace_mode != GST_VIDEO_INTERLACE_MODE_MIXED)
            decoder->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
        }
        gdouble frame_rate;

        gst_util_fraction_to_double (decoder->info.fps_n,
          decoder->info.fps_d, &frame_rate);
        if ((int)(frame_rate + 0.5) == 60)
          decoder->can_double_deinterlace = TRUE;
        decoder->enable_deinterlace = TRUE;

        break;
      }
      default:
        break;
    }

    /* Re-initialize shared decoder / VPP task with shared surface pool and
     * MFX session. This re-initialization can only occur if no other peer
     * MFX task from a downstream element marked the decoder task with
     * another task type at this point. */
    if ((decoder->enable_csc || decoder->enable_deinterlace)
        && (gst_mfx_task_get_task_type (decoder->decode) == GST_MFX_TASK_DECODER)) {
      if (!gst_mfx_decoder_reinit (decoder, &outsurf->Info))
        ret = GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;
      else
        ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
      goto end;
    }

    if (decoder->filter) {
      do {
        filter_sts = gst_mfx_filter_process (decoder->filter, surface,
          &filter_surface);
        queue_output_frame (decoder, filter_surface);
      } while (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == filter_sts);

      if (GST_MFX_FILTER_STATUS_SUCCESS != filter_sts) {
        GST_ERROR ("MFX post-processing error while decoding.");
        ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
        goto end;
      }
    }
    else {
      queue_output_frame (decoder, surface);
    }

    decoder->bitstream = g_byte_array_remove_range (decoder->bitstream, 0,
      decoder->bs.DataOffset);
    decoder->bs.DataOffset = 0;
    decoder->bs.Data = decoder->bitstream->data;
    decoder->bs.MaxLength = decoder->bitstream->len;

    ret = GST_MFX_DECODER_STATUS_SUCCESS;
  }

end:
  gst_buffer_unmap (frame->input_buffer, &minfo);

  return ret;
}

GstMfxDecoderStatus
gst_mfx_decoder_flush (GstMfxDecoder * decoder)
{
  GstMfxDecoderStatus ret;
  GstMfxFilterStatus filter_sts;
  GstMfxSurface *surface, *filter_surface;
  mfxFrameSurface1 *insurf, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts;

  g_return_val_if_fail(decoder != NULL, GST_MFX_DECODER_STATUS_FLUSHED);

  do {
    surface = gst_mfx_surface_new_from_pool (decoder->pool);
    if (!surface)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

    insurf = gst_mfx_surface_get_frame_surface (surface);
    sts = MFXVideoDECODE_DecodeFrameAsync (decoder->session, NULL,
        insurf, &outsurf, &syncp);
    GST_DEBUG ("MFXVideoDECODE_DecodeFrameAsync status: %d", sts);
    if (sts == MFX_WRN_DEVICE_BUSY)
      g_usleep (100);
  } while (MFX_WRN_DEVICE_BUSY == sts);

  if (syncp) {
    do {
      sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
      GST_DEBUG ("MFXVideoCORE_SyncOperation status: %d", sts);
    } while (MFX_WRN_IN_EXECUTION == sts);

    surface = gst_mfx_surface_pool_find_surface (decoder->pool, outsurf);

    if (decoder->filter) {
      do {
        filter_sts = gst_mfx_filter_process (decoder->filter, surface,
          &filter_surface);
        queue_output_frame (decoder, filter_surface);
      } while (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == filter_sts);
    }
    else {
      queue_output_frame (decoder, surface);
    }

    ret = GST_MFX_DECODER_STATUS_SUCCESS;
  } else {
    ret = GST_MFX_DECODER_STATUS_FLUSHED;
  }
  return ret;
}
