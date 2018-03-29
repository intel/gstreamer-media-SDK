/*
 *  Copyright (C) 2013-2014 Intel Corporation
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
#include "gstmfxenc.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideomemory.h"
#include "gstmfxvideobufferpool.h"

#include <gst-libs/mfx/gstmfxdisplay.h>

#define GST_PLUGIN_NAME "mfxencode"
#define GST_PLUGIN_DESC "A MFX-based video encoder"

GST_DEBUG_CATEGORY_STATIC (gst_mfxencode_debug);
#define GST_CAT_DEFAULT gst_mfxencode_debug

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstMfxEnc,
    gst_mfxenc, GST_TYPE_VIDEO_ENCODER, GST_MFX_PLUGIN_BASE_INIT_INTERFACES);

enum
{
  PROP_0,

  PROP_BASE,
};

static gboolean
gst_mfxenc_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  gboolean ret = TRUE;
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = gst_mfx_handle_context_query (query, plugin->aggregator);
      break;
    default:
      ret = GST_VIDEO_ENCODER_CLASS (gst_mfxenc_parent_class)->sink_query
          (encoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_mfxenc_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  gboolean ret = TRUE;
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = gst_mfx_handle_context_query (query, plugin->aggregator);
      break;
    default:
      ret = GST_VIDEO_ENCODER_CLASS (gst_mfxenc_parent_class)->src_query
          (encoder, query);
      break;
  }

  return ret;
}

typedef struct
{
  GstMfxEncoderProp id;
  GParamSpec *pspec;
  GValue value;
} PropValue;

static PropValue *
prop_value_new (GstMfxEncoderPropInfo * prop)
{
  static const GValue default_value = G_VALUE_INIT;
  PropValue *prop_value;

  if (!prop || !prop->pspec)
    return NULL;

  prop_value = g_slice_new (PropValue);
  if (!prop_value)
    return NULL;

  prop_value->id = prop->prop;
  prop_value->pspec = g_param_spec_ref (prop->pspec);

  memcpy (&prop_value->value, &default_value, sizeof (prop_value->value));
  g_value_init (&prop_value->value, prop->pspec->value_type);
  g_param_value_set_default (prop->pspec, &prop_value->value);
  return prop_value;
}

static void
prop_value_free (PropValue * prop_value)
{
  if (!prop_value)
    return;

  if (G_VALUE_TYPE (&prop_value->value))
    g_value_unset (&prop_value->value);

  if (prop_value->pspec) {
    g_param_spec_unref (prop_value->pspec);
    prop_value->pspec = NULL;
  }
  g_slice_free (PropValue, prop_value);
}

static inline PropValue *
prop_value_lookup (GstMfxEnc * encode, guint prop_id)
{
  GPtrArray *const prop_values = encode->prop_values;

  if (prop_values &&
      (prop_id >= PROP_BASE && prop_id < PROP_BASE + prop_values->len))
    return g_ptr_array_index (prop_values, prop_id - PROP_BASE);
  return NULL;
}

static gboolean
gst_mfxenc_default_get_property (GstMfxEnc * encode, guint prop_id,
    GValue * value)
{
  PropValue *const prop_value = prop_value_lookup (encode, prop_id);

  if (prop_value) {
    g_value_copy (&prop_value->value, value);
    return TRUE;
  }
  return FALSE;
}

static gboolean
gst_mfxenc_default_set_property (GstMfxEnc * encode, guint prop_id,
    const GValue * value)
{
  PropValue *const prop_value = prop_value_lookup (encode, prop_id);

  if (prop_value) {
    g_value_copy (value, &prop_value->value);
    return TRUE;
  }
  return FALSE;
}

static gboolean
ensure_output_state (GstMfxEnc * encode)
{
  GstVideoEncoder *const venc = GST_VIDEO_ENCODER_CAST (encode);
  GstMfxEncClass *const klass = GST_MFXENC_GET_CLASS (encode);
  GstMfxEncoderStatus status;
  GstCaps *out_caps;

  if (!encode->input_state_changed)
    return TRUE;

  out_caps = klass->get_caps (encode);
  if (!out_caps)
    return FALSE;

  if (encode->output_state)
    gst_video_codec_state_unref (encode->output_state);
  encode->output_state = gst_video_encoder_set_output_state (venc, out_caps,
      encode->input_state);

  if (encode->need_codec_data) {
    status = gst_mfx_encoder_get_codec_data (encode->encoder,
        &encode->output_state->codec_data);
    if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
      return FALSE;
  }

  if (!gst_video_encoder_negotiate (venc))
    return FALSE;

  encode->input_state_changed = FALSE;
  return TRUE;
}

static GstFlowReturn
gst_mfxenc_push_frame (GstMfxEnc * encode, GstVideoCodecFrame * out_frame)
{
  GstVideoEncoder *const venc = GST_VIDEO_ENCODER_CAST (encode);
  GstMfxEncClass *const klass = GST_MFXENC_GET_CLASS (encode);
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret;

  /* Update output state */
  if (!ensure_output_state (encode))
    goto error_output_state;

  if (klass->format_buffer) {
    ret = klass->format_buffer (encode, out_frame->output_buffer, &outbuf);
    if (GST_FLOW_OK != ret)
      goto error_format_buffer;
    if (outbuf) {
      gst_buffer_replace (&out_frame->output_buffer, outbuf);
      gst_buffer_unref (outbuf);
    }
  }

  GST_DEBUG ("output:%" GST_TIME_FORMAT ", size:%zu",
      GST_TIME_ARGS (out_frame->pts),
      gst_buffer_get_size (out_frame->output_buffer));

  return gst_video_encoder_finish_frame (venc, out_frame);
  /* ERRORS */
error_format_buffer:
  {
    GST_ERROR ("failed to format encoded buffer in system memory");
    if (out_frame->output_buffer)
      gst_buffer_unref (out_frame->output_buffer);
    gst_video_codec_frame_unref (out_frame);
    return ret;
  }
error_output_state:
  {
    GST_ERROR ("failed to negotiate output state");
    gst_video_codec_frame_unref (out_frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static GstCaps *
gst_mfxenc_get_caps_impl (GstVideoEncoder * venc)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (venc);
  GstCaps *caps;

  if (plugin->sinkpad_caps)
    caps = gst_caps_ref (plugin->sinkpad_caps);
  else
    caps = gst_pad_get_pad_template_caps (plugin->sinkpad);
  return caps;
}

static GstCaps *
gst_mfxenc_get_caps (GstVideoEncoder * venc, GstCaps * filter)
{
  GstCaps *caps, *out_caps;

  out_caps = gst_mfxenc_get_caps_impl (venc);
  if (out_caps && filter) {
    caps = gst_caps_intersect_full (out_caps, filter, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (out_caps);
    out_caps = caps;
  }
  return out_caps;
}

static gboolean
gst_mfxenc_destroy (GstMfxEnc * encode)
{
  if (encode->input_state) {
    gst_video_codec_state_unref (encode->input_state);
    encode->input_state = NULL;
  }

  if (encode->output_state) {
    gst_video_codec_state_unref (encode->output_state);
    encode->output_state = NULL;
  }
  gst_mfx_encoder_replace (&encode->encoder, NULL);
  return TRUE;
}

static gboolean
ensure_encoder (GstMfxEnc * encode)
{
  GstMfxEncClass *klass = GST_MFXENC_GET_CLASS (encode);
  GstMfxEncoderStatus status;
  GPtrArray *const prop_values = encode->prop_values;
  guint i;

  g_return_val_if_fail (klass->alloc_encoder, FALSE);

  if (encode->encoder)
    return TRUE;

  encode->encoder = klass->alloc_encoder (encode);
  if (!encode->encoder)
    return FALSE;

  if (prop_values) {
    for (i = 0; i < prop_values->len; i++) {
      PropValue *const prop_value = g_ptr_array_index (prop_values, i);
      status = gst_mfx_encoder_set_property (encode->encoder, prop_value->id,
          &prop_value->value);
      if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
        return FALSE;
    }
  }
  return TRUE;
}

static gboolean
gst_mfxenc_open (GstVideoEncoder * venc)
{
  return gst_mfx_plugin_base_ensure_aggregator (GST_MFX_PLUGIN_BASE (venc));
}

static gboolean
gst_mfxenc_close (GstVideoEncoder * venc)
{
  gst_mfx_plugin_base_close (GST_MFX_PLUGIN_BASE (venc));
  return TRUE;
}

static gboolean
gst_mfxenc_stop (GstVideoEncoder * venc)
{
  return gst_mfxenc_destroy (GST_MFXENC_CAST (venc));
}

static gboolean
set_codec_state (GstMfxEnc * encode, GstVideoCodecState * state)
{
  GstMfxEncClass *const klass = GST_MFXENC_GET_CLASS (encode);
  GstMfxEncoderStatus status;

  g_return_val_if_fail (encode->encoder, FALSE);

  /* Initialize codec specific parameters */
  if (klass->set_config && !klass->set_config (encode))
    return FALSE;

  status = gst_mfx_encoder_set_codec_state (encode->encoder, state);
  if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
    return FALSE;
  return TRUE;
}

static gboolean
gst_mfxenc_set_format (GstVideoEncoder * venc, GstVideoCodecState * state)
{
  GstMfxEnc *const encode = GST_MFXENC_CAST (venc);
  GstMfxEncoderStatus status;

  g_return_val_if_fail (state->caps != NULL, FALSE);

  if (!gst_mfx_plugin_base_set_caps (GST_MFX_PLUGIN_BASE (encode),
          state->caps, NULL))
    return FALSE;

  if (!ensure_encoder (encode))
    return FALSE;
  if (!set_codec_state (encode, state))
    return FALSE;

  status = gst_mfx_encoder_start (encode->encoder);
  if (GST_MFX_ENCODER_STATUS_SUCCESS != status)
    return FALSE;

  if (encode->input_state)
    gst_video_codec_state_unref (encode->input_state);
  encode->input_state = gst_video_codec_state_ref (state);
  encode->input_state_changed = TRUE;

  return TRUE;
}

static GstFlowReturn
gst_mfxenc_handle_frame (GstVideoEncoder * venc, GstVideoCodecFrame * frame)
{
  GstMfxEnc *const encode = GST_MFXENC_CAST (venc);
  GstMfxEncoderStatus status;
  GstMfxVideoMeta *meta;
  GstMfxSurface *surface;
  GstFlowReturn ret;
  GstBuffer *buf;

  ret = gst_mfx_plugin_base_get_input_buffer (GST_MFX_PLUGIN_BASE (encode),
      frame->input_buffer, &buf);
  if (ret != GST_FLOW_OK)
    goto error_buffer_invalid;

  gst_buffer_replace (&frame->input_buffer, buf);
  gst_buffer_unref (buf);

  meta = gst_buffer_get_mfx_video_meta (frame->input_buffer);
  if (!meta)
    goto error_buffer_no_meta;

  surface = gst_mfx_video_meta_get_surface (meta);
  if (!surface)
    goto error_buffer_no_surface;

  gst_video_codec_frame_set_user_data (frame,
      gst_mfx_surface_ref (surface), (GDestroyNotify) gst_mfx_surface_unref);

  status = gst_mfx_encoder_encode (encode->encoder, frame);
  if (status < GST_MFX_ENCODER_STATUS_SUCCESS)
    goto error_encode_frame;
  else if (status > 0) {
    ret = GST_FLOW_OK;
    goto done;
  }
  ret = gst_mfxenc_push_frame (encode, frame);
  gst_mfx_surface_dequeue(surface);

done:
  return ret;
  /* ERRORS */
error_buffer_invalid:
  {
    if (buf)
      gst_buffer_unref (buf);
    gst_video_codec_frame_unref (frame);
    return ret;
  }
error_buffer_no_meta:
  {
    GST_ERROR ("failed to get GstMfxVideoMeta information");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
error_buffer_no_surface:
  {
    GST_ERROR ("failed to get VA surface");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
error_encode_frame:
  {
    GST_ERROR ("failed to encode frame %d (status %d)",
        frame->system_frame_number, status);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_mfxenc_finish (GstVideoEncoder * venc)
{
  GstMfxEnc *const encode = GST_MFXENC_CAST (venc);
  GstMfxEncoderStatus status;
  GstVideoCodecFrame *frame;
  GstFlowReturn ret = GST_FLOW_ERROR;

  /* Return "not-negotiated" error since this means we did not even reach
   * GstVideoEncoder::set_format () state, where the encoder could have
   * been created */
  if (!encode->encoder)
    return GST_FLOW_NOT_NEGOTIATED;

  do {
    status = gst_mfx_encoder_flush (encode->encoder, &frame);
    if (GST_MFX_ENCODER_STATUS_SUCCESS != status)
      break;
    ret = gst_mfxenc_push_frame (encode, gst_video_codec_frame_ref (frame));
  } while (GST_FLOW_OK == ret);

  return ret;
}

static gboolean
gst_mfxenc_propose_allocation (GstVideoEncoder * venc, GstQuery * query)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (venc);

  if (!gst_mfx_plugin_base_propose_allocation (plugin, query))
    return FALSE;
  return TRUE;
}

static void
gst_mfxenc_finalize (GObject * object)
{
  GstMfxEnc *const encode = GST_MFXENC_CAST (object);

  gst_mfxenc_destroy (encode);

  if (encode->prop_values) {
    g_ptr_array_unref (encode->prop_values);
    encode->prop_values = NULL;
  }

  gst_mfx_plugin_base_finalize (GST_MFX_PLUGIN_BASE (object));
  G_OBJECT_CLASS (gst_mfxenc_parent_class)->finalize (object);
}

static void
gst_mfxenc_init (GstMfxEnc * encode)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (encode);

  gst_mfx_plugin_base_init (GST_MFX_PLUGIN_BASE (encode), GST_CAT_DEFAULT);

  gst_pad_use_fixed_caps (plugin->srcpad);
}

static void
gst_mfxenc_class_init (GstMfxEncClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  GstVideoEncoderClass *const venc_class = GST_VIDEO_ENCODER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_mfxencode_debug,
      GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

  gst_mfx_plugin_base_class_init (GST_MFX_PLUGIN_BASE_CLASS (klass));

  object_class->finalize = gst_mfxenc_finalize;

  venc_class->open = GST_DEBUG_FUNCPTR (gst_mfxenc_open);
  venc_class->stop = GST_DEBUG_FUNCPTR (gst_mfxenc_stop);
  venc_class->close = GST_DEBUG_FUNCPTR (gst_mfxenc_close);
  venc_class->set_format = GST_DEBUG_FUNCPTR (gst_mfxenc_set_format);
  venc_class->handle_frame = GST_DEBUG_FUNCPTR (gst_mfxenc_handle_frame);
  venc_class->finish = GST_DEBUG_FUNCPTR (gst_mfxenc_finish);
  venc_class->getcaps = GST_DEBUG_FUNCPTR (gst_mfxenc_get_caps);
  venc_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_mfxenc_propose_allocation);

  klass->get_property = gst_mfxenc_default_get_property;
  klass->set_property = gst_mfxenc_default_set_property;

  venc_class->src_query = GST_DEBUG_FUNCPTR (gst_mfxenc_src_query);
  venc_class->sink_query = GST_DEBUG_FUNCPTR (gst_mfxenc_sink_query);
}

static inline GPtrArray *
get_properties (GstMfxEncClass * klass)
{
  return klass->get_properties ? klass->get_properties () : NULL;
}

gboolean
gst_mfxenc_init_properties (GstMfxEnc * encode)
{
  GPtrArray *const props = get_properties (GST_MFXENC_GET_CLASS (encode));
  guint i;

  /* XXX: use base_init ()/base_finalize () to avoid multiple initializations */
  if (!props)
    return FALSE;

  encode->prop_values =
      g_ptr_array_new_full (props->len, (GDestroyNotify) prop_value_free);
  if (!encode->prop_values) {
    g_ptr_array_unref (props);
    return FALSE;
  }

  for (i = 0; i < props->len; i++) {
    PropValue *const prop_value = prop_value_new ((GstMfxEncoderPropInfo *)
        g_ptr_array_index (props, i));
    if (!prop_value)
      return FALSE;
    g_ptr_array_add (encode->prop_values, prop_value);
  }
  return TRUE;
}

gboolean
gst_mfxenc_class_init_properties (GstMfxEncClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  GPtrArray *const props = get_properties (klass);
  guint i;

  if (!props)
    return FALSE;

  for (i = 0; i < props->len; i++) {
    GstMfxEncoderPropInfo *const prop = g_ptr_array_index (props, i);
    g_object_class_install_property (object_class, PROP_BASE + i, prop->pspec);
  }
  g_ptr_array_unref (props);
  return TRUE;
}
