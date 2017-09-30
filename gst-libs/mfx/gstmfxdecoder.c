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
  GstObject parent_instance;

  GstMfxTaskAggregator *aggregator;
  GstMfxTask *decode;
  GstMfxProfile profile;
  GstMfxSurfacePool *pool;
  GstMfxFilter *filter;
  GByteArray *bitstream;
  GByteArray *codec_data;

  GQueue input_frames;
  GQueue decoded_frames;
  GQueue pending_frames;
  GQueue discarded_frames;

  mfxSession session;
  mfxVideoParam params;
  mfxFrameAllocRequest request;
  mfxBitstream bs;
  const mfxPluginUID *plugin_uid;

  GstVideoInfo info;
  gboolean inited;
  gboolean configured;
  gboolean was_reset;
  gboolean has_ready_frames;
  gboolean memtype_is_system;
  gboolean skip_corrupted_frames;
  gboolean is_autoplugged;
  gboolean can_double_deinterlace;
  guint num_partial_frames;
  guint initial_frame_latency;
  guint num_frame_latency;

  /* For special double frame rate deinterlacing case */
  GstClockTime current_pts;
  GstClockTime duration;
  GstClockTime pts_offset;
};

G_DEFINE_TYPE (GstMfxDecoder, gst_mfx_decoder, GST_TYPE_OBJECT);

void
gst_mfx_decoder_set_video_info (GstMfxDecoder * decoder, GstVideoInfo * info)
{
  g_return_if_fail (decoder != NULL);
  decoder->info = *info;
}

const mfxFrameAllocRequest *
gst_mfx_decoder_get_request (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, NULL);
  return &decoder->request;
}

gboolean
gst_mfx_decoder_get_frame (GstMfxDecoder * decoder,
    GstVideoCodecFrame ** out_frame, gboolean discarded)
{
  g_return_val_if_fail (decoder != NULL, FALSE);

  if (!discarded)
    *out_frame = g_queue_pop_tail (&decoder->decoded_frames);
  else
    *out_frame = g_queue_pop_tail (&decoder->discarded_frames);
  return *out_frame != NULL;
}

void
gst_mfx_decoder_skip_corrupted_frames (GstMfxDecoder * decoder)
{
  g_return_if_fail (decoder != NULL);
  decoder->skip_corrupted_frames = TRUE;
}

void
gst_mfx_decoder_decide_output_memtype (GstMfxDecoder * decoder,
    gboolean memtype_is_video)
{
  mfxVideoParam *params;

  g_return_if_fail (decoder != NULL);

  /* The decoder may be forced to use system memory by a downstream peer
   * MFX VPP task, or due to decoder limitations for that particular
   * codec. In that case, return to confirm the use of system memory */
  params = gst_mfx_task_get_video_params (decoder->decode);

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
}

static gboolean
init_decoder (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFX_ERR_NONE;

  /* Make sure frame allocator points to the right task to allocate surfaces */
  gst_mfx_task_aggregator_set_current_task (decoder->aggregator,
      decoder->decode);
  /* calls gst_mfx_task_frame_alloc() when configured with video memory */
  sts = MFXVideoDECODE_Init (decoder->session, &decoder->params);
  if (sts < 0) {
    GST_ERROR ("Error initializing the MFX video decoder %d", sts);
    return FALSE;
  }

  decoder->pool = gst_mfx_surface_pool_new_with_task (decoder->decode);
  if (!decoder->pool)
    return FALSE;

  return TRUE;
}

static void
close_decoder (GstMfxDecoder * decoder)
{
  gst_mfx_surface_pool_replace (&decoder->pool, NULL);
  /* Make sure frame allocator points to the right task to free surfaces */
  gst_mfx_task_aggregator_set_current_task (decoder->aggregator,
      decoder->decode);
  /* calls gst_mfx_task_frame_free() when configured with video memory */
  if (decoder->plugin_uid)
    MFXVideoUSER_UnLoad (decoder->session, decoder->plugin_uid);
  MFXVideoDECODE_Close (decoder->session);
}

static void
gst_mfx_decoder_finalize (GObject * object)
{
  GstMfxDecoder *decoder = GST_MFX_DECODER (object);

  g_byte_array_unref (decoder->bitstream);
  if (decoder->codec_data)
    g_byte_array_unref (decoder->codec_data);

  g_queue_foreach (&decoder->input_frames,
      (GFunc) gst_video_codec_frame_unref, NULL);
  g_queue_foreach (&decoder->pending_frames,
      (GFunc) gst_video_codec_frame_unref, NULL);
  g_queue_foreach (&decoder->decoded_frames,
      (GFunc) gst_video_codec_frame_unref, NULL);
  g_queue_clear (&decoder->input_frames);
  g_queue_clear (&decoder->pending_frames);
  g_queue_clear (&decoder->decoded_frames);
  g_queue_clear (&decoder->discarded_frames);

  gst_mfx_filter_replace (&decoder->filter, NULL);
  close_decoder (decoder);
  gst_mfx_task_aggregator_unref (decoder->aggregator);
  gst_mfx_task_replace (&decoder->decode, NULL);
}

static mfxStatus
gst_mfx_decoder_configure_plugins (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFX_ERR_NONE;

  switch (decoder->profile.codec) {
    case MFX_CODEC_HEVC:{
      guint i;
      const mfxPluginUID *uids[] = {
        &MFX_PLUGINID_HEVCD_HW,
        &MFX_PLUGINID_HEVCD_SW,
      };

      for (i = 0; i < sizeof (uids) / sizeof (uids[0]); i++) {

#if MSDK_CHECK_VERSION(1,19)
        /* skip hw decoder on platforms before broadwell for hevc main-10 content */
        if (decoder->profile.profile == MFX_PROFILE_HEVC_MAIN10
            && gst_mfx_task_aggregator_get_platform (decoder->aggregator) <
            MFX_PLATFORM_BROADWELL && uids[i] == &MFX_PLUGINID_HEVCD_HW)
          continue;
#endif

        decoder->plugin_uid = uids[i];
        sts = MFXVideoUSER_Load (decoder->session, decoder->plugin_uid, 1);
        if (MFX_ERR_NONE == sts) {
          if (uids[i] == &MFX_PLUGINID_HEVCD_SW)
            decoder->params.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
          break;
        }
      }
      break;
    }
    case MFX_CODEC_VP8:
      decoder->plugin_uid = &MFX_PLUGINID_VP8D_HW;
      sts = MFXVideoUSER_Load (decoder->session, decoder->plugin_uid, 1);

      break;
#if MSDK_CHECK_VERSION(1,19)
    case MFX_CODEC_VP9:
      decoder->plugin_uid = &MFX_PLUGINID_VP9D_HW;
      sts = MFXVideoUSER_Load (decoder->session, decoder->plugin_uid, 1);

      break;
#endif
    default:
      sts = MFX_ERR_NONE;
  }
  return sts;
}

static void
gst_mfx_decoder_reconfigure_params (GstMfxDecoder * decoder)
{
  mfxFrameInfo *frame_info = &decoder->params.mfx.FrameInfo;

  frame_info->PicStruct = GST_VIDEO_INFO_IS_INTERLACED (&decoder->info) ?
      (GST_VIDEO_INFO_FLAG_IS_SET (&decoder->info,
          GST_VIDEO_FRAME_FLAG_TFF) ? MFX_PICSTRUCT_FIELD_TFF :
      MFX_PICSTRUCT_FIELD_BFF)
      : MFX_PICSTRUCT_PROGRESSIVE;

  if (decoder->profile.codec == MFX_CODEC_VP8
#if MSDK_CHECK_VERSION(1,19)
      || decoder->profile.codec == MFX_CODEC_VP9
#endif
      ) {
    frame_info->Width = GST_ROUND_UP_16 (decoder->info.width);
    frame_info->Height = GST_ROUND_UP_16 (decoder->info.height);
  } else {
    frame_info->Width = GST_ROUND_UP_32 (decoder->info.width);
    frame_info->Height = GST_ROUND_UP_32 (decoder->info.height);
  }

  frame_info->FrameRateExtN = decoder->info.fps_n;
  frame_info->FrameRateExtD = decoder->info.fps_d;

  /* We may need to overallocate surfaces when used with decodebin
   * TODO: Figure out why jerky playback issues occur with decodebin
   * and remove this hack */
  if (decoder->is_autoplugged) {
    decoder->params.mfx.CodecLevel = 0;
    decoder->params.mfx.MaxDecFrameBuffering = 0;
  }

  if (frame_info->FourCC == MFX_FOURCC_P010
      && (decoder->plugin_uid == &MFX_PLUGINID_HEVCD_HW
#if MSDK_CHECK_VERSION(1,19)
          || decoder->plugin_uid == &MFX_PLUGINID_VP9D_HW
#endif
      )) {
    frame_info->Shift = 1;
  }
}

static gboolean
task_init (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFX_ERR_NONE;

  decoder->decode =
      gst_mfx_task_new (decoder->aggregator, GST_MFX_TASK_DECODER);
  if (!decoder->decode)
    return FALSE;

  decoder->session = gst_mfx_task_get_session (decoder->decode);

  sts = gst_mfx_decoder_configure_plugins (decoder);
  if (sts < 0) {
    GST_ERROR ("Unable to load plugin %d", sts);
    goto error_load_plugin;
  }

  return TRUE;

error_load_plugin:
  {
    gst_mfx_task_unref (decoder->decode);
    return FALSE;
  }
}

static gboolean
gst_mfx_decoder_create (GstMfxDecoder * decoder,
    GstMfxTaskAggregator * aggregator, GstMfxProfile profile,
    const GstVideoInfo * info, GByteArray * codec_data, mfxU16 async_depth,
    gboolean live_mode, gboolean is_autoplugged)
{
  decoder->is_autoplugged = is_autoplugged;
  decoder->profile = profile;
  decoder->info = *info;
  if (!decoder->info.fps_n)
    decoder->info.fps_n = 30;
  decoder->duration =
      (decoder->info.fps_d / (gdouble) decoder->info.fps_n) * 1000000000;

  decoder->params.mfx.CodecId = profile.codec;
  decoder->params.AsyncDepth = is_autoplugged ? 16 : async_depth;
  if (live_mode) {
    decoder->params.AsyncDepth = 1;
    decoder->bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    /* This is a special fix for Android Auto / Apple Carplay issues */
    if (decoder->params.mfx.CodecId == MFX_CODEC_AVC)
      decoder->params.mfx.DecodedOrder = 1;
  }
  decoder->params.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  decoder->bitstream = g_byte_array_new ();
  if (!decoder->bitstream)
    return FALSE;

  if (codec_data) {
    decoder->codec_data = g_byte_array_sized_new (codec_data->len);
    decoder->codec_data = g_byte_array_append (decoder->codec_data,
        codec_data->data, codec_data->len);
  }

  decoder->aggregator = gst_mfx_task_aggregator_ref (aggregator);
  if (!task_init (decoder))
    goto error_init_task;
  return TRUE;

error_init_task:
  {
    g_byte_array_unref (decoder->bitstream);
    return FALSE;
  }
}

static void
gst_mfx_decoder_init (GstMfxDecoder * decoder)
{
  decoder->configured = FALSE;
  decoder->inited = FALSE;
  decoder->pts_offset = GST_CLOCK_TIME_NONE;

  g_queue_init (&decoder->input_frames);
  g_queue_init (&decoder->decoded_frames);
  g_queue_init (&decoder->pending_frames);
  g_queue_init (&decoder->discarded_frames);
}

static void
gst_mfx_decoder_class_init (GstMfxDecoderClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gst_mfx_decoder_finalize;
}

GstMfxDecoder *
gst_mfx_decoder_new (GstMfxTaskAggregator * aggregator, GstMfxProfile profile,
    const GstVideoInfo * info, GByteArray * codec_data, mfxU16 async_depth,
    gboolean live_mode, gboolean is_autoplugged)
{
  GstMfxDecoder *decoder;

  g_return_val_if_fail (aggregator != NULL, NULL);
  g_return_val_if_fail (info != NULL, NULL);

  decoder = g_object_new (GST_TYPE_MFX_DECODER, NULL);
  if (!decoder)
    return NULL;

  if (!gst_mfx_decoder_create (decoder, aggregator, profile, info, codec_data,
          async_depth, live_mode, is_autoplugged))
    goto error;
  return decoder;

error:
  gst_mfx_decoder_unref (decoder);
  return NULL;
}

GstMfxDecoder *
gst_mfx_decoder_ref (GstMfxDecoder * decoder)
{
  g_return_val_if_fail (decoder != NULL, NULL);

  return gst_object_ref (GST_OBJECT (decoder));
}

void
gst_mfx_decoder_unref (GstMfxDecoder * decoder)
{
  gst_object_unref (GST_OBJECT (decoder));
}

void
gst_mfx_decoder_replace (GstMfxDecoder ** old_decoder_ptr,
    GstMfxDecoder * new_decoder)
{
  g_return_if_fail (old_decoder_ptr != NULL);

  gst_object_replace ((GstObject **) old_decoder_ptr, GST_OBJECT (new_decoder));
}

static gboolean
configure_filter (GstMfxDecoder * decoder)
{
  mfxU32 output_format =
      gst_video_format_to_mfx_fourcc (GST_VIDEO_INFO_FORMAT (&decoder->info));
  mfxU32 decode_format = decoder->params.mfx.FrameInfo.FourCC;
  gboolean enable_csc = FALSE, enable_deinterlace = FALSE;

  if (output_format != decode_format)
    enable_csc = TRUE;
  if (GST_VIDEO_INFO_IS_INTERLACED (&decoder->info)) {
    gdouble frame_rate;

    gst_util_fraction_to_double (decoder->info.fps_n,
        decoder->info.fps_d, &frame_rate);
    if ((int) (frame_rate + 0.5) == 60)
      decoder->can_double_deinterlace = TRUE;
    enable_deinterlace = TRUE;
  }

  if (enable_csc || enable_deinterlace) {
    decoder->filter = gst_mfx_filter_new_with_task (decoder->aggregator,
        decoder->decode, GST_MFX_TASK_VPP_IN,
        decoder->memtype_is_system, decoder->memtype_is_system);
    if (!decoder->filter) {
      GST_ERROR ("Unable to initialize filter.");
      return FALSE;
    }

    decoder->request.Type |=
        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    decoder->request.NumFrameSuggested += (1 - decoder->params.AsyncDepth);

    gst_mfx_task_set_request (decoder->decode, &decoder->request);

    gst_mfx_filter_set_frame_info (decoder->filter,
        &decoder->params.mfx.FrameInfo);
    if (enable_csc)
      gst_mfx_filter_set_format (decoder->filter, output_format);
    if (enable_deinterlace) {
      GstMfxDeinterlaceMethod di_method = decoder->can_double_deinterlace ?
          GST_MFX_DEINTERLACE_METHOD_ADVANCED_NOREF
          : GST_MFX_DEINTERLACE_METHOD_ADVANCED;
      gst_mfx_filter_set_deinterlace_method (decoder->filter, di_method);
    }
    gst_mfx_filter_set_async_depth (decoder->filter,
        decoder->params.AsyncDepth);

    if (!gst_mfx_filter_prepare (decoder->filter)) {
      GST_ERROR ("Unable to set up postprocessing filter.");
      goto error;
    }
  }
  return TRUE;

error:
  gst_mfx_filter_unref (decoder->filter);
  return FALSE;
}

static GstMfxDecoderStatus
gst_mfx_decoder_start (GstMfxDecoder * decoder)
{
  /* Get updated video params if modified by peer MFX element */
  gst_mfx_task_update_video_params (decoder->decode, &decoder->params);

  if (decoder->params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
    decoder->memtype_is_system = FALSE;
    gst_mfx_task_use_video_memory (decoder->decode);
  } else {
    decoder->memtype_is_system = TRUE;
    gst_mfx_task_ensure_memtype_is_system (decoder->decode);
  }

  if (!decoder->filter
      && gst_mfx_task_get_task_type (decoder->decode) == GST_MFX_TASK_DECODER) {
    if (!configure_filter (decoder))
      return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;
  }

  if (!init_decoder (decoder))
    return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;

  GST_INFO ("Initialized MFX decoder task using %s memory",
      decoder->memtype_is_system ? "system" : "video");

  return GST_MFX_DECODER_STATUS_SUCCESS;
}

static GstMfxDecoderStatus
gst_mfx_decoder_prepare (GstMfxDecoder * decoder)
{
  GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_CONFIGURED;
  GstVideoCodecFrame *cur_frame = NULL;
  GstMapInfo minfo = { 0 };
  guint i = 0;
  mfxStatus sts = MFX_ERR_NONE;

  do {
    if (MFX_CODEC_VC1 == decoder->profile.codec
        && MFX_PROFILE_VC1_ADVANCED == decoder->profile.profile) {
      decoder->bitstream = g_byte_array_append (decoder->bitstream,
          decoder->codec_data->data, decoder->codec_data->len);
      decoder->bs.Data = decoder->bitstream->data;
      decoder->bs.DataLength = decoder->codec_data->len;
      decoder->bs.MaxLength = decoder->bs.DataLength;
    } else {
      cur_frame = g_queue_peek_nth (&decoder->input_frames, i++);
      if (!cur_frame)
        return GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
      if (!gst_buffer_map (cur_frame->input_buffer, &minfo, GST_MAP_READ)) {
        GST_ERROR ("Failed to map input buffer");
        return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
      }

      if (minfo.size) {
        decoder->bs.DataLength = decoder->bs.MaxLength = minfo.size;
        decoder->bs.DataOffset = 0;
        decoder->bs.Data = minfo.data;
      }
    }

    sts = MFXVideoDECODE_DecodeHeader (decoder->session, &decoder->bs,
        &decoder->params);
    if (MFX_ERR_NONE == sts) {
      ret = GST_MFX_DECODER_STATUS_CONFIGURED;
      break;
    }
    else if (MFX_ERR_MORE_DATA == sts) {
      ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
    } else if (sts < 0) {
      GST_ERROR ("Decode header error %d\n", sts);
      ret = GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER;
      goto done;
    }
    gst_buffer_unmap (cur_frame->input_buffer, &minfo);
  } while (cur_frame);

  gst_mfx_decoder_reconfigure_params (decoder);

  sts = MFXVideoDECODE_QueryIOSurf (decoder->session, &decoder->params,
      &decoder->request);
  if (sts < 0) {
    GST_ERROR ("Unable to query decode allocation request %d", sts);
    ret = GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER;
    goto done;
  } else if (sts == MFX_WRN_PARTIAL_ACCELERATION) {
    decoder->params.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  }

  decoder->memtype_is_system =
      ! !(decoder->params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
  decoder->request.Type = decoder->memtype_is_system ?
      MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

  if (decoder->memtype_is_system)
    gst_mfx_task_ensure_memtype_is_system (decoder->decode);

  gst_mfx_task_set_request (decoder->decode, &decoder->request);
  gst_mfx_task_set_video_params (decoder->decode, &decoder->params);
  decoder->configured = TRUE;

  memset (&decoder->bs, 0, sizeof (mfxBitstream));
  if (MFX_CODEC_VC1 == decoder->profile.codec
      && MFX_PROFILE_VC1_ADVANCED == decoder->profile.profile)
    decoder->bs.DataOffset = 1;

done:
  if (cur_frame)
    gst_buffer_unmap (cur_frame->input_buffer, &minfo);
  return ret;
}

gboolean
gst_mfx_decoder_reset (GstMfxDecoder * decoder)
{
  mfxStatus sts = MFX_ERR_NONE;

  g_queue_foreach (&decoder->input_frames,
      (GFunc) gst_video_codec_frame_unref, NULL);
  g_queue_clear (&decoder->input_frames);

  /* Flush pending frames */
  while (!g_queue_is_empty (&decoder->pending_frames))
    g_queue_push_head (&decoder->discarded_frames,
        g_queue_pop_head (&decoder->pending_frames));

  decoder->pts_offset = GST_CLOCK_TIME_NONE;
  decoder->current_pts = 0;

  if (decoder->bitstream->len)
    g_byte_array_remove_range (decoder->bitstream, 0, decoder->bitstream->len);
  memset (&decoder->bs, 0, sizeof (mfxBitstream));

  decoder->was_reset = TRUE;
  decoder->has_ready_frames = FALSE;
  decoder->num_partial_frames = 0;
  decoder->num_frame_latency = 0;

  sts = MFXVideoDECODE_Reset (decoder->session, &decoder->params);

  return (MFX_ERR_NONE == sts);
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

  gst_video_codec_frame_set_user_data (out_frame,
      gst_mfx_surface_ref (surface), (GDestroyNotify) gst_mfx_surface_unref);
  g_queue_push_head (&decoder->decoded_frames, out_frame);

  GST_LOG ("decoded frame : %u",
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
gst_mfx_decoder_decode (GstMfxDecoder * decoder, GstVideoCodecFrame * frame)
{
  GstMapInfo minfo = { 0 };
  GstVideoCodecFrame *input_frame = NULL;
  GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_SUCCESS;
  GstMfxFilterStatus filter_sts;
  GstMfxSurface *surface, *filter_surface;
  mfxFrameSurface1 *insurf = NULL, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;

  if (!GST_CLOCK_TIME_IS_VALID (decoder->pts_offset)
      && GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)
      && GST_CLOCK_TIME_IS_VALID (frame->pts))
    decoder->pts_offset = frame->pts;

  if (!decoder->can_double_deinterlace) {
    /* Save frames for later synchronization with decoded MFX surfaces */
    g_queue_insert_sorted (&decoder->pending_frames, frame, sort_pts, NULL);
  } else {
    g_queue_push_head (&decoder->discarded_frames, frame);
  }

  /* Keep a reference to the incoming frame so that we can read from  it later */
  g_queue_push_tail (&decoder->input_frames, gst_video_codec_frame_ref (frame));

  if (G_UNLIKELY (!decoder->inited)) {
    if (!decoder->configured)
      ret = gst_mfx_decoder_prepare (decoder);
    else
      ret = gst_mfx_decoder_start (decoder);

    if (GST_MFX_DECODER_STATUS_SUCCESS == ret)
      decoder->inited = TRUE;
    else
      goto end;
  }

  while (input_frame = g_queue_pop_head (&decoder->input_frames)) {
    if (!gst_buffer_map (input_frame->input_buffer, &minfo, GST_MAP_READ)) {
      GST_ERROR ("Failed to map input buffer");
      ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
      goto end;
    }

    if (decoder->was_reset) {
      if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (input_frame)) {
        /* Sequence header check for I-frames after MPEG2 video seeking */
        if (MFX_CODEC_MPEG2 == decoder->profile.codec) {
          decoder->bs.Data = minfo.data;
          decoder->bs.DataLength = decoder->bs.MaxLength = minfo.size;

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
        } else if (MFX_CODEC_VC1 == decoder->profile.codec
            && MFX_PROFILE_VC1_ADVANCED == decoder->profile.profile) {
          decoder->bitstream = g_byte_array_append (decoder->bitstream,
              decoder->codec_data->data, decoder->codec_data->len);
          decoder->bs.DataLength += decoder->codec_data->len;
          /* Don't ask me why for VC1 this is required after seeking */
          decoder->bs.DataOffset = 1;
        }
        decoder->was_reset = FALSE;
      } else {
        ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
        goto end;
      }
    }

    if (minfo.size) {
      decoder->bitstream = g_byte_array_append (decoder->bitstream,
          minfo.data, minfo.size);
      decoder->bs.DataLength += minfo.size;
      decoder->bs.MaxLength = decoder->bs.DataLength + decoder->bs.DataOffset;
      decoder->bs.Data = decoder->bitstream->data;
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
      if (decoder->has_ready_frames && !decoder->can_double_deinterlace) {
        decoder->num_partial_frames++;
      } else {
        if (!GST_VIDEO_INFO_IS_INTERLACED (&decoder->info)) {
          decoder->num_frame_latency++;
          if (decoder->initial_frame_latency
              && decoder->num_frame_latency > decoder->initial_frame_latency)
            decoder->num_partial_frames =
                decoder->num_frame_latency - decoder->initial_frame_latency;
        }
      }
      ret = GST_MFX_DECODER_STATUS_ERROR_MORE_DATA;
      goto end;
    }
    else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts) {
      ret = GST_MFX_DECODER_STATUS_REINIT;
      goto end;
    }

    if (MFX_ERR_NONE != sts) {
      GST_ERROR ("Status %d : Error during MFX decoding", sts);
      ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
      goto end;
    }

    if (syncp) {
      if (!decoder->initial_frame_latency)
        decoder->initial_frame_latency = decoder->num_frame_latency + 1;

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
          if (decoder->info.interlace_mode ==
              GST_VIDEO_INTERLACE_MODE_PROGRESSIVE && decoder->has_ready_frames)
            decoder->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
          else {
            if (decoder->info.interlace_mode != GST_VIDEO_INTERLACE_MODE_MIXED)
              decoder->info.interlace_mode =
                  GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
          }
          break;
        }
        default:
          break;
      }

      if (!decoder->can_double_deinterlace && decoder->num_partial_frames) {
        GstVideoCodecFrame *cur_frame;
        guint n = g_queue_get_length (&decoder->pending_frames) - 1;

        /* Discard partial frames */
        while (decoder->num_partial_frames
            && (cur_frame = g_queue_peek_nth (&decoder->pending_frames, n))) {
          if ((cur_frame->pts - decoder->pts_offset) % decoder->duration) {
            g_queue_push_head (&decoder->discarded_frames,
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

      surface = gst_mfx_surface_pool_find_surface (decoder->pool, outsurf);

      if (!gst_mfx_task_has_type (decoder->decode, GST_MFX_TASK_ENCODER)) {
        do {
          sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
          if (MFX_ERR_NONE != sts && sts < 0) {
            GST_ERROR ("MFXVideoCORE_SyncOperation() error status: %d", sts);
            ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
            goto end;
          }
        } while (MFX_WRN_IN_EXECUTION == sts);
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
      } else {
        queue_output_frame (decoder, surface);
      }

      decoder->has_ready_frames = TRUE;
      decoder->bitstream = g_byte_array_remove_range (decoder->bitstream, 0,
          decoder->bs.DataOffset);
      decoder->bs.DataOffset = 0;

      ret = GST_MFX_DECODER_STATUS_SUCCESS;
    }

    gst_buffer_unmap (input_frame->input_buffer, &minfo);
    gst_video_codec_frame_unref (input_frame);
  }

end:
  if (input_frame) {
    gst_buffer_unmap (input_frame->input_buffer, &minfo);
    gst_video_codec_frame_unref (input_frame);
  }

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

  g_return_val_if_fail (decoder != NULL, GST_MFX_DECODER_STATUS_FLUSHED);

  do {
    surface = gst_mfx_surface_new_from_pool (decoder->pool);
    if (!surface)
      return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

    insurf = gst_mfx_surface_get_frame_surface (surface);
    sts = MFXVideoDECODE_DecodeFrameAsync (decoder->session, NULL,
        insurf, &outsurf, &syncp);
    GST_DEBUG ("MFXVideoDECODE_DecodeFrameAsync() status: %d", sts);
    if (sts == MFX_WRN_DEVICE_BUSY)
      g_usleep (100);
  } while (MFX_WRN_DEVICE_BUSY == sts);

  if (syncp) {
    do {
      sts = MFXVideoCORE_SyncOperation (decoder->session, syncp, 1000);
      if (MFX_ERR_NONE != sts && sts < 0) {
        GST_ERROR ("MFXVideoCORE_SyncOperation() error status: %d", sts);
        return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
      }
    } while (MFX_WRN_IN_EXECUTION == sts);

    surface = gst_mfx_surface_pool_find_surface (decoder->pool, outsurf);

    if (decoder->filter) {
      do {
        filter_sts = gst_mfx_filter_process (decoder->filter, surface,
            &filter_surface);
        queue_output_frame (decoder, filter_surface);
      } while (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == filter_sts);
    } else {
      queue_output_frame (decoder, surface);
    }

    ret = GST_MFX_DECODER_STATUS_SUCCESS;
  } else {
    ret = GST_MFX_DECODER_STATUS_FLUSHED;
  }
  return ret;
}
