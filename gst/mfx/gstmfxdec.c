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

#define DEFAULT_ASYNC_DEPTH 16

/* Default templates */
#define GST_CAPS_CODEC(CODEC) CODEC "; "

/* *INDENT-OFF* */
static const char gst_mfxdecode_sink_caps_str[] =
    GST_CAPS_CODEC ("video/mpeg, \
        mpegversion = 2")
    GST_CAPS_CODEC ("video/x-h264, \
        alignment = (string) au, \
        stream-format = (string) byte-stream")
    GST_CAPS_CODEC ("video/x-h265, \
        alignment = (string) au, \
        stream-format = (string) byte-stream")
    GST_CAPS_CODEC ("video/x-wmv")
    GST_CAPS_CODEC ("video/x-vp8")
#ifdef HAS_VP9
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
  PROP_LIVE_MODE
};

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (gst_mfxdecode_sink_caps_str)
);

static GstStaticPadTemplate src_template_factory =
  GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (gst_mfxdecode_src_caps_str)
);

G_DEFINE_TYPE_WITH_CODE (
    GstMfxDec,
    gst_mfxdec,
    GST_TYPE_VIDEO_DECODER,
    GST_MFX_PLUGIN_BASE_INIT_INTERFACES);


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

  if (feature == GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED)
    return FALSE;

  switch (feature) {
  case GST_MFX_CAPS_FEATURE_MFX_SURFACE:
    features =
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_MFX_SURFACE, NULL);
    break;
  default:
    break;
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

  gst_mfx_decoder_use_video_memory (mfxdec->decoder, !plugin->srcpad_caps_is_raw);

  mfxdec->do_renego = FALSE;

  return TRUE;
}

static void
gst_mfxdec_set_property (GObject * object, guint prop_id,
  const GValue * value, GParamSpec * pspec)
{
  GstMfxDec *dec;

  g_return_if_fail (GST_IS_MFXDEC (object));
  dec = GST_MFXDEC (object);

  GST_DEBUG_OBJECT (object, "gst_mfx_dec_set_property");
  switch (prop_id) {
  case PROP_ASYNC_DEPTH:
    dec->async_depth = g_value_get_uint (value);
    break;
  case PROP_LIVE_MODE:
    dec->live_mode = g_value_get_boolean (value);
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
  GstMfxDec *dec;

  g_return_if_fail (GST_IS_MFXDEC (object));
  dec = GST_MFXDEC (object);

  switch (prop_id) {
  case PROP_ASYNC_DEPTH:
    g_value_set_uint (value, dec->async_depth);
    break;
  case PROP_LIVE_MODE:
    g_value_set_boolean (value, dec->live_mode);
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

  if (!gst_mfxdec_update_src_caps (mfxdec))
    return FALSE;

  if (!gst_video_info_from_caps (&info, mfxdec->srcpad_caps))
    return FALSE;

  /* live streaming configuration cannot be used with VC1 or MPEG2 */
  if (gst_mfx_profile_get_codec(profile) == MFX_CODEC_MPEG2 ||
      gst_mfx_profile_get_codec(profile) == MFX_CODEC_VC1)
    mfxdec->live_mode = FALSE;

  plugin->srcpad_caps_is_raw =
      gst_mfx_query_peer_has_raw_caps (GST_VIDEO_DECODER_SRC_PAD (mfxdec));

  mfxdec->decoder = gst_mfx_decoder_new (plugin->aggregator,
      profile, &info, mfxdec->async_depth, plugin->srcpad_caps_is_raw,
      mfxdec->live_mode);
  if (!mfxdec->decoder)
    return FALSE;

  mfxdec->do_renego = TRUE;

  return TRUE;
}

static gboolean
gst_mfxdec_reset_full (GstMfxDec * mfxdec, GstCaps * caps,
  gboolean hard)
{
  GstMfxProfile profile;

  if (mfxdec->decoder && !hard) {
    profile = gst_mfx_profile_from_caps (caps);
    if (profile == gst_mfx_decoder_get_profile (mfxdec->decoder)) {
      gst_mfx_decoder_reset (mfxdec->decoder);
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
  G_OBJECT_CLASS (gst_mfxdec_parent_class)->finalize (object);
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

  if (info->interlace_mode == GST_VIDEO_INTERLACE_MODE_MIXED ||
      gst_mfx_profile_get_codec(profile) == MFX_CODEC_MPEG2 ||
      gst_mfx_profile_get_codec(profile) == MFX_CODEC_JPEG)
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

  if (!gst_mfxdec_negotiate (mfxdec))
      goto not_negotiated;

  GST_LOG ("Received new data of size %" G_GSIZE_FORMAT
      ", dts %" GST_TIME_FORMAT
      ", pts:%" GST_TIME_FORMAT
      ", dur:%" GST_TIME_FORMAT,
      gst_buffer_get_size (frame->input_buffer),
      GST_TIME_ARGS (frame->dts),
      GST_TIME_ARGS (frame->pts),
      GST_TIME_ARGS (frame->duration));

  sts = gst_mfx_decoder_decode (mfxdec->decoder, frame);

  switch (sts) {
    case GST_MFX_DECODER_STATUS_ERROR_NO_DATA:
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

error_decode:
  {
    GST_ERROR ("MFX decode error %d", sts);
    gst_video_decoder_drop_frame (vdec, frame);
    return GST_FLOW_NOT_SUPPORTED;
  }
not_negotiated:
  {
    GST_ERROR_OBJECT (mfxdec, "not negotiated");
    gst_video_decoder_drop_frame (vdec, frame);
    return GST_FLOW_NOT_SUPPORTED;
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
    sts = gst_mfx_decoder_flush (mfxdec->decoder, &out_frame);
    if (GST_MFX_DECODER_STATUS_FLUSHED == sts)
      break;
    ret = gst_mfxdec_push_decoded_frame (mfxdec, out_frame);
  } while (GST_MFX_DECODER_STATUS_SUCCESS == sts);
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
      ret = GST_VIDEO_DECODER_CLASS (gst_mfxdec_parent_class)->sink_query (vdec, query);
      break;
    }
  }

  return ret;
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
      ret = GST_VIDEO_DECODER_CLASS (gst_mfxdec_parent_class)->src_query (vdec, query);
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

  GST_DEBUG_CATEGORY_INIT (mfxdec_debug, GST_PLUGIN_NAME,
      0, GST_PLUGIN_DESC);

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


  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template_factory));
  gst_element_class_set_static_metadata (element_class,
      "MFX Video Decoder",
      "Codec/Decoder/Video",
      "Uses libmfx for decoding video streams",
      "Ishmael Sameen<ishmael.visayana.sameen@intel.com>");

  vdec_class->open = GST_DEBUG_FUNCPTR (gst_mfxdec_open);
  vdec_class->close = GST_DEBUG_FUNCPTR (gst_mfxdec_close);
  vdec_class->flush = GST_DEBUG_FUNCPTR (gst_mfxdec_flush);
  vdec_class->finish = GST_DEBUG_FUNCPTR (gst_mfxdec_finish);
  vdec_class->set_format = GST_DEBUG_FUNCPTR (gst_mfxdec_set_format);
  vdec_class->handle_frame = GST_DEBUG_FUNCPTR (gst_mfxdec_handle_frame);
  vdec_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_mfxdec_decide_allocation);
  vdec_class->src_query = GST_DEBUG_FUNCPTR (gst_mfxdec_src_query);
  vdec_class->sink_query = GST_DEBUG_FUNCPTR (gst_mfxdec_sink_query);
}

static void
gst_mfxdec_init (GstMfxDec *mfxdec)
{
  mfxdec->async_depth = DEFAULT_ASYNC_DEPTH;
  mfxdec->live_mode = FALSE;

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (mfxdec), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (mfxdec), TRUE);
}
