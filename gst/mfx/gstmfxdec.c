/*
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#include "gst-libs/mfx/sysdeps.h"
#include "gstmfxdec.h"
#include "gstmfxvideomemory.h"
#include "gstmfxvideobufferpool.h"
#include "gstmfxpluginutil.h"

#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst-libs/mfx/gstmfxprofile.h>

#define GST_PLUGIN_NAME "mfxdecode"
#define GST_PLUGIN_DESC "MFX Video Decoder"

GST_DEBUG_CATEGORY_STATIC (mfxdec_debug);
#define GST_CAT_DEFAULT mfxdec_debug

#define GST_MFXDEC_PARAMS_QDATA \
  g_quark_from_static_string("mfxdec-params")

#define DEFAULT_ASYNC_DEPTH 4

/* Default templates */
#define GST_CAPS_CODEC(CODEC) CODEC "; "

static const char gst_mfxdecode_sink_caps_str[] =
    GST_CAPS_CODEC ("video/x-h264, \
        parsed = true, \
        alignment = (string) au, \
        profile = (string) { constrained-baseline, baseline, main, high }, \
        stream-format = (string) { avc, byte-stream }")
#ifdef USE_HEVC_10BIT_DECODER
    GST_CAPS_CODEC ("video/x-h265, \
        alignment = (string) au, \
        profile = (string) { main, main-10 }, \
        profile = (string) { main }, \
        stream-format = (string) byte-stream")
#else
    GST_CAPS_CODEC ("video/x-h265, \
        alignment = (string) au, \
        profile = (string) { main }, \
        stream-format = (string) byte-stream")
#endif
    GST_CAPS_CODEC ("video/mpeg, \
        mpegversion = 2")
    GST_CAPS_CODEC ("video/x-wmv, \
        stream-format = (string) { sequence-layer-frame-layer, bdu }")
    GST_CAPS_CODEC ("video/x-vp8")
#ifdef USE_VP9_DECODER
    GST_CAPS_CODEC ("video/x-vp9")
#endif
    GST_CAPS_CODEC ("image/jpeg")
  ;

static const char gst_mfxdecode_src_caps_str[] =
  GST_MFX_MAKE_SURFACE_CAPS ";"
  GST_VIDEO_CAPS_MAKE ("{ NV12, BGRA }");

enum
{
  PROP_0,
  PROP_ASYNC_DEPTH,
  PROP_LIVE_MODE,
  PROP_SKIP_CORRUPTED_FRAMES
};

static GstStaticPadTemplate src_template_factory =
  GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (gst_mfxdecode_src_caps_str)
);

typedef struct _GstMfxCodecMap GstMfxCodecMap;
struct _GstMfxCodecMap
{
  const gchar *name;
  guint rank;
  const gchar *caps_str;
};

static const GstMfxCodecMap mfx_codec_map[] = {
  {"h264", GST_RANK_PRIMARY + 3,
      "video/x-h264, \
       parsed = true, \
       alignment = (string) au, \
       profile = (string) { constrained-baseline, baseline, main, high }, \
       stream-format = (string) { avc, byte-stream }"},
#ifdef USE_HEVC_10BIT_DECODER
  {"hevc", GST_RANK_PRIMARY + 3,
      "video/x-h265, \
       alignment = (string) au, \
       profile = (string) { main, main-10 }, \
       stream-format = (string) byte-stream"},
#else
#ifdef USE_HEVC_DECODER
  {"hevc", GST_RANK_PRIMARY + 3,
      "video/x-h265, \
       alignment = (string) au, \
       profile = (string) main, \
       stream-format = (string) byte-stream"},
#endif
#endif
  {"mpeg2", GST_RANK_PRIMARY + 3,
      "video/mpeg, \
       mpegversion=2, \
       systemstream=(boolean) false"},
  {"vc1", GST_RANK_PRIMARY + 3,
      "video/x-wmv, \
       stream-format = (string) { sequence-layer-frame-layer, bdu }"},
# ifdef USE_VP8_DECODER
  {"vp8", GST_RANK_PRIMARY + 3, "video/x-vp8"},
# endif
# ifdef USE_VP9_DECODER
  {"vp9", GST_RANK_PRIMARY + 3, "video/x-vp9"},
# endif
  {"jpeg", GST_RANK_PRIMARY + 3, "image/jpeg"},
  {NULL, GST_RANK_NONE, gst_mfxdecode_sink_caps_str},
};

static GstElementClass *parent_class = NULL;

static GstVideoCodecState *
copy_video_codec_state (const GstVideoCodecState * in_state)
{
  GstVideoCodecState *state;

  g_return_val_if_fail (in_state != NULL, NULL);

  state = g_slice_new0 (GstVideoCodecState);
  state->ref_count = 1;
  state->info = in_state->info;
  state->caps = gst_caps_copy (in_state->caps);
  if (in_state->codec_data)
    state->codec_data = gst_buffer_copy_deep (in_state->codec_data);

  return state;
}

static gboolean
gst_mfxdec_input_state_replace (GstMfxDec * mfxdec,
  const GstVideoCodecState * new_state)
{
  if (mfxdec->input_state) {
    if (new_state) {
      const GstCaps *curcaps = mfxdec->input_state->caps;
      /* If existing caps are equal of the new state, keep the
       * existing state without renegotiating. */
      if (gst_caps_is_strictly_equal (curcaps, new_state->caps)) {
        GST_DEBUG ("Ignoring new caps %" GST_PTR_FORMAT
            " since are equal to current ones", new_state->caps);
        return FALSE;
      }
    }
    gst_video_codec_state_unref (mfxdec->input_state);
  }

  if (new_state)
    mfxdec->input_state = copy_video_codec_state (new_state);
  else
    mfxdec->input_state = NULL;

  return TRUE;
}

static inline gboolean
gst_mfxdec_update_sink_caps (GstMfxDec * mfxdec, GstCaps * caps)
{
  GST_INFO_OBJECT (mfxdec, "new sink caps = %" GST_PTR_FORMAT, caps);
  gst_caps_replace (&mfxdec->sinkpad_caps, caps);
  return TRUE;
}

static gboolean
gst_mfxdec_update_src_caps (GstMfxDec * mfxdec)
{
  GstVideoDecoder *const vdec = GST_VIDEO_DECODER (mfxdec);
  GstVideoCodecState *state, *ref_state;
  GstVideoInfo *vi;
  GstVideoFormat format;
  GstCapsFeatures *features = NULL;
  GstMfxCapsFeature feature;

  if (!mfxdec->input_state)
    return FALSE;

  ref_state = mfxdec->input_state;

  feature =
      gst_mfx_find_preferred_caps_feature (GST_VIDEO_DECODER_SRC_PAD (vdec),
      &format);

  if (GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED == feature)
    return FALSE;

  if (GST_MFX_CAPS_FEATURE_MFX_SURFACE == feature) {
    features =
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_MFX_SURFACE, NULL);
  }

  state = gst_video_decoder_set_output_state (vdec, format,
      ref_state->info.width, ref_state->info.height, ref_state);
  if (!state || state->info.width == 0 || state->info.height == 0)
    return FALSE;

  vi = &state->info;

  state->caps = gst_video_info_to_caps (vi);
  if (features)
    gst_caps_set_features (state->caps, 0, features);
  GST_INFO_OBJECT (mfxdec, "new src caps = %" GST_PTR_FORMAT, state->caps);
  gst_caps_replace (&mfxdec->srcpad_caps, state->caps);
  gst_video_codec_state_unref (state);

  return TRUE;
}

static gboolean
gst_mfxdec_negotiate (GstMfxDec * mfxdec)
{
  GstVideoDecoder *const vdec = GST_VIDEO_DECODER (mfxdec);
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (vdec);

  if (!mfxdec->do_renego)
    return TRUE;

  GST_DEBUG_OBJECT (mfxdec, "Input codec state changed, doing renegotiation");

  if (!gst_mfx_plugin_base_set_caps (plugin, mfxdec->sinkpad_caps, NULL))
    return FALSE;
  if (!gst_mfxdec_update_src_caps (mfxdec))
    return FALSE;
  if (!gst_video_decoder_negotiate (vdec))
    return FALSE;
  if (!gst_mfx_plugin_base_set_caps (plugin, NULL, mfxdec->srcpad_caps))
    return FALSE;

  /* Final check to determine if system or video memory should be used for
   * the output of the decoder */
  gst_mfx_decoder_should_use_video_memory (mfxdec->decoder,
    !plugin->srcpad_caps_is_raw);

  mfxdec->do_renego = FALSE;

  return TRUE;
}

static void
gst_mfxdec_set_property (GObject * object, guint prop_id,
  const GValue * value, GParamSpec * pspec)
{
  GstMfxDec *dec = GST_MFXDEC (object);

  switch (prop_id) {
  case PROP_ASYNC_DEPTH:
    dec->async_depth = g_value_get_uint (value);
    break;
  case PROP_LIVE_MODE:
    dec->live_mode = g_value_get_boolean (value);
    break;
  case PROP_SKIP_CORRUPTED_FRAMES:
    dec->skip_corrupted_frames = g_value_get_boolean (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gst_mfxdec_get_property (GObject * object, guint prop_id, GValue * value,
  GParamSpec * pspec)
{
  GstMfxDec *dec = GST_MFXDEC (object);

  switch (prop_id) {
  case PROP_ASYNC_DEPTH:
    g_value_set_uint (value, dec->async_depth);
    break;
  case PROP_LIVE_MODE:
    g_value_set_boolean (value, dec->live_mode);
    break;
  case PROP_SKIP_CORRUPTED_FRAMES:
    g_value_set_boolean (value, dec->skip_corrupted_frames);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static gboolean
gst_mfxdec_decide_allocation (GstVideoDecoder * vdec, GstQuery * query)
{
  return gst_mfx_plugin_base_decide_allocation (GST_MFX_PLUGIN_BASE (vdec),
            query);
}

static gboolean
gst_mfxdec_create (GstMfxDec * mfxdec, GstCaps * caps)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (mfxdec);
  GstMfxProfile profile = gst_mfx_profile_from_caps (caps);
  GstVideoInfo info;
  GstObject *parent;
  GstBuffer *codec_data = NULL;
  gboolean is_in_avc = FALSE;

  if (!gst_mfxdec_update_src_caps (mfxdec))
    return FALSE;

  if (!gst_video_info_from_caps (&info, mfxdec->srcpad_caps))
    return FALSE;

  if (mfxdec->input_state) {
    GstStructure *structure = gst_caps_get_structure (mfxdec->input_state->caps, 0);
    if (structure && gst_structure_has_field_typed(structure, "stream-format",
          G_TYPE_STRING)) {
      const gchar *stream_format = gst_structure_get_string (structure, "stream-format");
      is_in_avc = (stream_format != NULL) && (g_strcmp0(stream_format, "avc") == 0);
    }
    codec_data = mfxdec->input_state->codec_data;
  }

  /* Increase async depth considerably when using decodebin to avoid
   * jerky video playback resulting from threading issues */
  parent = gst_object_get_parent(GST_OBJECT(mfxdec));
  if (parent && !GST_IS_PIPELINE (GST_ELEMENT(parent)))
    mfxdec->async_depth = 16;
  gst_object_replace (&parent, NULL);

  mfxdec->decoder = gst_mfx_decoder_new (plugin->aggregator, profile, &info,
      mfxdec->async_depth, mfxdec->live_mode, is_in_avc, codec_data);
  if (!mfxdec->decoder)
    return FALSE;

  if (mfxdec->skip_corrupted_frames)
    gst_mfx_decoder_skip_corrupted_frames (mfxdec->decoder);

  mfxdec->do_renego = TRUE;

  return TRUE;
}

static void
gst_mfxdec_flush_discarded_frames (GstMfxDec * mfxdec)
{
  GstVideoCodecFrame *frame = NULL;

  frame = gst_mfx_decoder_get_discarded_frame(mfxdec->decoder);
  while (frame) {
    GST_VIDEO_CODEC_FRAME_SET_DECODE_ONLY(frame);
    gst_video_decoder_finish_frame (GST_VIDEO_DECODER (mfxdec), frame);
    frame = gst_mfx_decoder_get_discarded_frame(mfxdec->decoder);
  }
}

static gboolean
gst_mfxdec_reset_full (GstMfxDec * mfxdec, GstCaps * caps,
  gboolean hard)
{
  GstMfxProfile profile;

  mfxdec->prev_surf = NULL;
  mfxdec->dequeuing = FALSE;
  mfxdec->flushing = 0;

  if (mfxdec->decoder && !hard) {
    profile = gst_mfx_profile_from_caps (caps);
    if (profile == gst_mfx_decoder_get_profile (mfxdec->decoder)) {
      gst_mfx_decoder_reset (mfxdec->decoder);
      gst_mfxdec_flush_discarded_frames (mfxdec);
      return TRUE;
    }
  }
  gst_mfx_decoder_replace (&mfxdec->decoder, NULL);

  return gst_mfxdec_create (mfxdec, caps);
}

static void
gst_mfxdec_finalize (GObject * object)
{
  GstMfxDec *const mfxdec = GST_MFXDEC (object);

  gst_caps_replace (&mfxdec->sinkpad_caps, NULL);
  gst_caps_replace (&mfxdec->srcpad_caps, NULL);

  gst_mfx_plugin_base_finalize (GST_MFX_PLUGIN_BASE (object));
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_mfxdec_open (GstVideoDecoder * vdec)
{
  return gst_mfx_plugin_base_ensure_aggregator (GST_MFX_PLUGIN_BASE (vdec));
}

static gboolean
gst_mfxdec_close (GstVideoDecoder * vdec)
{
  GstMfxDec *const mfxdec = GST_MFXDEC (vdec);

  gst_mfxdec_input_state_replace (mfxdec, NULL);
  gst_mfx_decoder_replace (&mfxdec->decoder, NULL);
  gst_mfx_plugin_base_close (GST_MFX_PLUGIN_BASE (mfxdec));

  return TRUE;
}

static gboolean
gst_mfxdec_flush (GstVideoDecoder * vdec)
{
  GstMfxDec *const mfxdec = GST_MFXDEC (vdec);
  GstMfxProfile profile = gst_mfx_decoder_get_profile (mfxdec->decoder);
  GstVideoInfo *info = gst_mfx_decoder_get_video_info (mfxdec->decoder);
  gboolean hard = FALSE;

  g_return_val_if_fail (info != NULL, FALSE);

  if (info->interlace_mode == GST_VIDEO_INTERLACE_MODE_MIXED
      && gst_mfx_profile_get_codec(profile) == MFX_CODEC_AVC)
    hard = TRUE;

  return gst_mfxdec_reset_full (mfxdec, mfxdec->sinkpad_caps, hard);
}

static gboolean
gst_mfxdec_set_format (GstVideoDecoder * vdec, GstVideoCodecState * state)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (vdec);
  GstMfxDec *const mfxdec = GST_MFXDEC (vdec);

  if (!gst_mfxdec_input_state_replace (mfxdec, state))
    return TRUE;
  if (!gst_mfxdec_update_sink_caps (mfxdec, state->caps))
    return FALSE;
  if (!gst_mfx_plugin_base_set_caps (plugin, mfxdec->sinkpad_caps, NULL))
    return FALSE;
  if (!gst_mfxdec_reset_full (mfxdec, mfxdec->sinkpad_caps, FALSE))
    return FALSE;

  return TRUE;
}


static GstFlowReturn
gst_mfxdec_push_decoded_frame (GstMfxDec *mfxdec, GstVideoCodecFrame * frame)
{
  GstMfxVideoMeta *meta;
  const GstMfxRectangle *crop_rect;
  GstMfxSurface *surface;

  surface = gst_video_codec_frame_get_user_data(frame);

  frame->output_buffer =
      gst_video_decoder_allocate_output_buffer (GST_VIDEO_DECODER (mfxdec));
  if (!frame->output_buffer)
    goto error_create_buffer;

  meta = gst_buffer_get_mfx_video_meta (frame->output_buffer);
  if (!meta)
    goto error_get_meta;
  gst_mfx_video_meta_set_surface (meta, surface);
  crop_rect = gst_mfx_surface_get_crop_rect (surface);
  if (crop_rect) {
    GstVideoCropMeta *const crop_meta =
      gst_buffer_add_video_crop_meta (frame->output_buffer);
    if (crop_meta) {
      crop_meta->x = crop_rect->x;
      crop_meta->y = crop_rect->y;
      crop_meta->width = crop_rect->width;
      crop_meta->height = crop_rect->height;
    }
  }

#if GST_CHECK_VERSION(1,8,0)
  gst_mfx_plugin_base_export_dma_buffer (GST_MFX_PLUGIN_BASE (mfxdec),
      frame->output_buffer);
#endif
  gst_mfx_surface_queue(surface);
  mfxdec->prev_surf = surface;

  return gst_video_decoder_finish_frame (GST_VIDEO_DECODER (mfxdec), frame);
  /* ERRORS */
error_create_buffer:
  {
    gst_video_decoder_drop_frame (GST_VIDEO_DECODER (mfxdec), frame);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
error_get_meta:
  {
    gst_video_decoder_drop_frame (GST_VIDEO_DECODER (mfxdec), frame);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_mfxdec_handle_frame (GstVideoDecoder *vdec, GstVideoCodecFrame * frame)
{
  GstMfxDec *mfxdec = GST_MFXDEC (vdec);
  GstMfxDecoderStatus sts;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoCodecFrame *out_frame = NULL;
  gint cnt = 0;

  if (!gst_mfxdec_negotiate (mfxdec))
      goto not_negotiated;

  GST_LOG_OBJECT (mfxdec, "Received new data of size %" G_GSIZE_FORMAT
      ", dts %" GST_TIME_FORMAT
      ", pts:%" GST_TIME_FORMAT
      ", dur:%" GST_TIME_FORMAT,
      gst_buffer_get_size (frame->input_buffer),
      GST_TIME_ARGS (frame->dts),
      GST_TIME_ARGS (frame->pts),
      GST_TIME_ARGS (frame->duration));

  if (mfxdec->prev_surf && mfxdec->dequeuing
      && gst_mfx_decoder_need_sync_surface_out (mfxdec->decoder)) {
    cnt = frame->duration / 100000;
    while (!g_atomic_int_get(&mfxdec->flushing) && (cnt > 0) &&
        gst_mfx_surface_is_queued(mfxdec->prev_surf)) {
      g_usleep(100);
      --cnt;
    }
  }

  sts = gst_mfx_decoder_decode (mfxdec->decoder, frame);

  gst_mfxdec_flush_discarded_frames (mfxdec);

  switch (sts) {
    case GST_MFX_DECODER_STATUS_ERROR_MORE_DATA:
      ret = GST_FLOW_OK;
      break;
    case GST_MFX_DECODER_STATUS_SUCCESS:
      while (gst_mfx_decoder_get_decoded_frames(mfxdec->decoder, &out_frame)) {
        ret = gst_mfxdec_push_decoded_frame (mfxdec, out_frame);
        if (ret != GST_FLOW_OK)
          break;
      }
      break;
    case GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED:
    case GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER:
      goto error_decode;
    default:
      ret = GST_FLOW_ERROR;
  }
  return ret;
  /* ERRORS */
error_decode:
  {
    GST_ERROR_OBJECT (mfxdec, "MFX decode error %d", sts);
    gst_video_decoder_drop_frame (vdec, frame);
    return GST_FLOW_NOT_SUPPORTED;
  }
not_negotiated:
  {
    GST_ERROR_OBJECT (mfxdec, "not negotiated");
    gst_video_decoder_drop_frame (vdec, frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static GstFlowReturn
gst_mfxdec_finish (GstVideoDecoder *vdec)
{
  GstMfxDec *mfxdec = GST_MFXDEC (vdec);
  GstMfxDecoderStatus sts;
  GstVideoCodecFrame *out_frame;
  GstFlowReturn ret = GST_FLOW_OK;

  do {
    sts = gst_mfx_decoder_flush (mfxdec->decoder);
    if (GST_MFX_DECODER_STATUS_FLUSHED == sts)
      break;
    while (gst_mfx_decoder_get_decoded_frames(mfxdec->decoder, &out_frame)) {
      ret = gst_mfxdec_push_decoded_frame (mfxdec, out_frame);
      if (ret != GST_FLOW_OK)
        break;
    }
  } while (GST_MFX_DECODER_STATUS_SUCCESS == sts);

  gst_mfxdec_flush_discarded_frames (mfxdec);

  return ret;
}

static gboolean
gst_mfxdec_sink_query (GstVideoDecoder * vdec, GstQuery * query)
{
  gboolean ret = TRUE;
  GstMfxDec *mfxdec = GST_MFXDEC (vdec);
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (mfxdec);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      ret = gst_mfx_handle_context_query (query, plugin->aggregator);
      break;
    }
    default:{
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (vdec, query);
      break;
    }
  }
  return ret;
}

static gboolean
gst_mfxdec_sink_event (GstVideoDecoder * vdec, GstEvent * event)
{
  GstMfxDec *mfxdec = GST_MFXDEC (vdec);

  if (GST_EVENT_TYPE(event) == GST_EVENT_FLUSH_START)
    g_atomic_int_set(&mfxdec->flushing, 1);

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (vdec, event);
}

static gboolean
gst_mfxdec_src_event (GstVideoDecoder * vdec, GstEvent * event)
{
  GstMfxDec *mfxdec = GST_MFXDEC (vdec);

  if (GST_EVENT_TYPE(event) == GST_EVENT_LATENCY)
    mfxdec->dequeuing = TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_event (vdec, event);
}

static gboolean
gst_mfxdec_src_query (GstVideoDecoder * vdec, GstQuery * query)
{
  gboolean ret = TRUE;
  GstMfxDec *mfxdec = GST_MFXDEC (vdec);
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (mfxdec);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:{
      GstCaps *caps, *filter = NULL;
      GstPad *pad = GST_VIDEO_DECODER_SRC_PAD (vdec);

      gst_query_parse_caps (query, &filter);
      caps = gst_pad_get_pad_template_caps (pad);

      if (filter) {
        GstCaps *tmp = caps;
        caps = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref (tmp);
      }

      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      break;
    }
    case GST_QUERY_CONTEXT:{
      ret = gst_mfx_handle_context_query (query, plugin->aggregator);
      break;
    }
    default:{
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->src_query (vdec, query);
      break;
    }
  }
  return ret;
}

static void
gst_mfxdec_class_init (GstMfxDecClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *vdec_class = GST_VIDEO_DECODER_CLASS (klass);
  GstPadTemplate *pad_template;
  GstMfxCodecMap *map;
  gchar *name, *longname, *description;
  GstCaps *caps;

  GST_DEBUG_CATEGORY_INIT (mfxdec_debug, GST_PLUGIN_NAME,
      0, GST_PLUGIN_DESC);

  parent_class = g_type_class_peek_parent (klass);

  gst_mfx_plugin_base_class_init (GST_MFX_PLUGIN_BASE_CLASS (klass));

  gobject_class->set_property = gst_mfxdec_set_property;
  gobject_class->get_property = gst_mfxdec_get_property;
  gobject_class->finalize = gst_mfxdec_finalize;

  g_object_class_install_property (gobject_class, PROP_ASYNC_DEPTH,
  g_param_spec_uint ("async-depth", "Asynchronous Depth",
      "Number of async operations before explicit sync",
      0, 20, DEFAULT_ASYNC_DEPTH,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LIVE_MODE,
  g_param_spec_boolean ("live-mode",
      "Live Streaming Mode",
      "Live streaming mode (not recommended)",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SKIP_CORRUPTED_FRAMES,
  g_param_spec_boolean ("skip-corrupted-frames",
      "Skip corrupted frames",
      "Skip decoded frames that have major corruption",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  vdec_class->open = GST_DEBUG_FUNCPTR (gst_mfxdec_open);
  vdec_class->close = GST_DEBUG_FUNCPTR (gst_mfxdec_close);
  vdec_class->flush = GST_DEBUG_FUNCPTR (gst_mfxdec_flush);
  vdec_class->finish = GST_DEBUG_FUNCPTR (gst_mfxdec_finish);
  vdec_class->set_format = GST_DEBUG_FUNCPTR (gst_mfxdec_set_format);
  vdec_class->handle_frame = GST_DEBUG_FUNCPTR (gst_mfxdec_handle_frame);
  vdec_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_mfxdec_decide_allocation);
  vdec_class->src_query = GST_DEBUG_FUNCPTR (gst_mfxdec_src_query);
  vdec_class->src_event = GST_DEBUG_FUNCPTR (gst_mfxdec_src_event);
  vdec_class->sink_query = GST_DEBUG_FUNCPTR (gst_mfxdec_sink_query);
  vdec_class->sink_event = GST_DEBUG_FUNCPTR (gst_mfxdec_sink_event);

  map = (GstMfxCodecMap *) g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass),
      GST_MFXDEC_PARAMS_QDATA);

  if (map->name) {
    name = g_ascii_strup (map->name, -1);
    longname = g_strdup_printf ("MFX %s decoder", name);
    description = g_strdup_printf ("An MFX-based %s video decoder", name);
    g_free (name);
  }
  else {
    longname = g_strdup_printf ("MFX Video Decoder");
    description = g_strdup_printf ("Uses libmfx for decoding video streams");
  }

  gst_element_class_set_static_metadata (element_class, longname,
      "Codec/Decoder/Video", description,
      "Ishmael Sameen<ishmael.visayana.sameen@intel.com>");

  g_free (longname);
  g_free (description);

  /* sink pad */
  caps = gst_caps_from_string (map->caps_str);
  pad_template = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      caps);
  gst_caps_unref (caps);
  gst_element_class_add_pad_template (element_class, pad_template);

  /* src pad */
  gst_element_class_add_static_pad_template (element_class,
      &src_template_factory);
}

static void
gst_mfxdec_init (GstMfxDec *mfxdec)
{
  mfxdec->async_depth = DEFAULT_ASYNC_DEPTH;
  mfxdec->live_mode = FALSE;
  mfxdec->skip_corrupted_frames = FALSE;
  mfxdec->prev_surf = NULL;
  mfxdec->dequeuing = FALSE;
  mfxdec->flushing = 0;

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (mfxdec), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (mfxdec), TRUE);
}

gboolean
gst_mfxdec_register (GstPlugin * plugin)
{
  gboolean ret = FALSE;
  guint i, rank;
  gchar *type_name, *element_name;
  const gchar *name;
  GType type;
  GTypeInfo typeinfo = {
    sizeof (GstMfxDecClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_mfxdec_class_init,
    NULL,
    NULL,
    sizeof (GstMfxDec),
    0,
    (GInstanceInitFunc) gst_mfxdec_init,
  };

  for (i = 0; i < G_N_ELEMENTS (mfx_codec_map); i++) {
    name = mfx_codec_map[i].name;
    rank = mfx_codec_map[i].rank;

    if (name) {
      type_name = g_strdup_printf ("GstMfxDec_%s", name);
      element_name = g_strdup_printf ("mfx%sdec", name);
    }
    else {
      type_name = g_strdup_printf ("GstMfxDec");
      element_name = g_strdup_printf ("mfxdecode");
    }

    type = g_type_from_name (type_name);
    if (!type) {
      /* create the gtype now */
      type = g_type_register_static (GST_TYPE_VIDEO_DECODER, type_name,
          &typeinfo, 0);
      gst_mfx_plugin_base_init_interfaces (type);
      g_type_set_qdata (type, GST_MFXDEC_PARAMS_QDATA,
          (gpointer) & mfx_codec_map[i]);
    }

    /* mfxdecode was only registered for legacy purposes */
    ret |= gst_element_register (plugin, element_name, rank, type);

    g_free (element_name);
    g_free (type_name);
  }

  return ret;
}

