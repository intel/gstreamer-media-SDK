/*
 *  Copyright (C) 2012-2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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
#include <gst/video/video.h>
#include <gst/video/colorbalance.h>

#include "gstmfxpostproc.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideobufferpool.h"
#include "gstmfxvideomemory.h"

#define GST_PLUGIN_NAME "mfxvpp"
#define GST_PLUGIN_DESC "A video postprocessing filter"

GST_DEBUG_CATEGORY_STATIC (gst_debug_mfxpostproc);
#define GST_CAT_DEFAULT gst_debug_mfxpostproc

static void
gst_mfxpostproc_color_balance_iface_init (GstColorBalanceInterface * iface);

/* Default templates */
static const char gst_mfxpostproc_sink_caps_str[] =
    GST_MFX_MAKE_SURFACE_CAPS "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (
        GST_CAPS_FEATURE_MEMORY_MFX_SURFACE ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        "{ NV12, BGRA }") ";"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_MFX_SUPPORTED_INPUT_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE (GST_MFX_SUPPORTED_INPUT_FORMATS);

static const char gst_mfxpostproc_src_caps_str[] =
    GST_MFX_MAKE_SURFACE_CAPS "; "
    GST_VIDEO_CAPS_MAKE ("{ NV12, BGRA }");

static GstStaticPadTemplate gst_mfxpostproc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mfxpostproc_sink_caps_str));

static GstStaticPadTemplate gst_mfxpostproc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mfxpostproc_src_caps_str));

G_DEFINE_TYPE_WITH_CODE (GstMfxPostproc,
    gst_mfxpostproc,
    GST_TYPE_BASE_TRANSFORM,
    GST_MFX_PLUGIN_BASE_INIT_INTERFACES
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_mfxpostproc_color_balance_iface_init));

enum
{
  PROP_0,

  PROP_ASYNC_DEPTH,
  PROP_FORMAT,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FORCE_ASPECT_RATIO,
  PROP_DEINTERLACE_MODE,
  PROP_DENOISE,
  PROP_DETAIL,
  PROP_HUE,
  PROP_SATURATION,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
  PROP_ROTATION,
  PROP_FRAMERATE,
  PROP_FRC_ALGORITHM,
};

#define DEFAULT_ASYNC_DEPTH             0
#define DEFAULT_FORMAT                  GST_VIDEO_FORMAT_NV12
#define DEFAULT_DEINTERLACE_MODE        GST_MFX_DEINTERLACE_MODE_BOB
#define DEFAULT_ROTATION                GST_MFX_ROTATION_0
#define DEFAULT_FRC_ALG                 GST_MFX_FRC_NONE
#define DEFAULT_BRIGHTNESS              0.0
#define DEFAULT_SATURATION              1.0
#define DEFAULT_HUE                     0.0
#define DEFAULT_CONTRAST                1.0

GType
gst_mfx_rotation_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue rotation_values[] = {
    {GST_MFX_ROTATION_0,
        "Unrotated", "0"},
    {GST_MFX_ROTATION_90,
        "Rotate by 90 degrees clockwise", "90"},
    {GST_MFX_ROTATION_180,
        "Rotate by 180  degrees clockwise", "180"},
    {GST_MFX_ROTATION_270,
        "Rotate by 270  degrees clockwise", "270"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&g_type)) {
    GType type = g_enum_register_static ("GstMfxRotation", rotation_values);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}

GType
gst_mfx_deinterlace_mode_get_type (void)
{
  static GType deinterlace_mode_type = 0;

  static const GEnumValue mode_types[] = {
    {GST_MFX_DEINTERLACE_MODE_BOB,
        "Bob deinterlacing", "bob"},
    {GST_MFX_DEINTERLACE_MODE_ADVANCED,
        "Advanced deinterlacing", "adi"},
    {GST_MFX_DEINTERLACE_MODE_ADVANCED_NOREF,
        "Advanced deinterlacing with no reference", "adi-noref"},
#if MSDK_CHECK_VERSION(1,19)
    {GST_MFX_DEINTERLACE_MODE_ADVANCED_SCD,
        "Advanced deinterlacing with scene change detection", "adi-scd"},
    {GST_MFX_DEINTERLACE_MODE_FIELD_WEAVING,
        "Field weaving", "weave"},
#endif // MSDK_CHECK_VERSION
    {0, NULL, NULL},
  };

  if (!deinterlace_mode_type) {
    deinterlace_mode_type =
        g_enum_register_static ("GstMfxDeinterlaceMode", mode_types);
  }
  return deinterlace_mode_type;
}

GType
gst_mfx_frc_algorithm_get_type (void)
{
  static GType alg = 0;
  static const GEnumValue frc_alg[] = {
    {GST_MFX_FRC_NONE,
        "No framerate conversion algorithm", "0"},
    {GST_MFX_FRC_PRESERVE_TIMESTAMP,
        "FRC with preserved original timestamps.", "frc-preserve-ts"},
    {GST_MFX_FRC_DISTRIBUTED_TIMESTAMP,
        "FRC with distributed timestamps.", "frc-distributed-ts"},
#if 0
    { GST_MFX_FRC_FRAME_INTERPOLATION,
       "Frame interpolation FRC.", "fi"},
       { GST_MFX_FRC_FI_PRESERVE_TIMESTAMP,
       "Frame dropping/repetition and frame interpolation FRC with preserved original timestamps.", "fi-preserve-ts"},
       { GST_MFX_FRC_FI_DISTRIBUTED_TIMESTAMP,
       "Frame dropping/repetition and frame interpolation FRC with distributed timestamps.", "fi-distributed-ts"},
#endif
    {0, NULL, NULL},
  };
  if (!alg)
    alg = g_enum_register_static ("GstMfxFrcAlgorithm", frc_alg);
  return alg;
}


/* ------------------------------------------------------------------------ */
/* --- GstColorBalance implementation                                   --- */
/* ------------------------------------------------------------------------ */

enum {
  CB_HUE = 1,
  CB_SATURATION,
  CB_BRIGHTNESS,
  CB_CONTRAST
};

typedef struct {
  guint cb_id;
  const gchar *channel_name;
  gfloat min_val;
  gfloat max_val;
} ColorBalanceMap;

static const ColorBalanceMap cb_map[4] = {
  {CB_HUE, "HUE", -1800.0, 1800.0},
  {CB_SATURATION, "SATURATION", 0.0, 1000.0},
  {CB_BRIGHTNESS, "BRIGHTNESS", -1000.0, 1000.0},
  {CB_CONTRAST, "CONTRAST", 0.0, 1000.0}
};

static GstColorBalanceType
gst_mfxpostproc_color_balance_get_type (GstColorBalance *cb)
{
  return GST_COLOR_BALANCE_HARDWARE;
}

static void
cb_channels_init (GstMfxPostproc * vpp)
{
  GstColorBalanceChannel *channel;
  guint i;
  for (i = 0; i < G_N_ELEMENTS(cb_map); i++) {
    channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
    channel->label = g_strdup (cb_map[i].channel_name);
    channel->min_value = cb_map[i].min_val;
    channel->max_value = cb_map[i].max_val;
    vpp->channels = g_list_prepend (vpp->channels, channel);
  }
}

static void
cb_channels_finalize (GstMfxPostproc * vpp)
{
  if (vpp->channels) {
    g_list_free_full (vpp->channels, g_object_unref);
    vpp->channels = NULL;
  }
}

static void
gst_mfxpostproc_color_balance_set_value (GstColorBalance * cb,
    GstColorBalanceChannel * channel, gint value)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (cb);
  gfloat new_val = value;

  /* To get the right value reflected from the gtk-play
   * color balance slider, the value must be normalized.
   * gtk-play set's the default value for all color balance
   * properties to 0.5. Since the default value for saturation
   * and contrast in MediaSDK is 1.0 with range of 0 to 10 with 0.01
   * increment, the value need to normalized to 0-1 for
   * value less than 500 and 1-10 for value more than 500. */

  if (g_ascii_strcasecmp (channel->label, "HUE") == 0) {
    new_val = (new_val / 10.0);

    if (vpp->hue != new_val) {
      vpp->hue = new_val;
      vpp->flags |= GST_MFX_POSTPROC_FLAG_HUE;
      vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_HUE;
    }
  } else if (g_ascii_strcasecmp (channel->label, "SATURATION") == 0) {
    if (new_val < 500 )
      new_val = ((1 - 0.0)/(500.0 - 0.0) * (new_val));
    else if (new_val > 500 )
      new_val = ((10.0 - 1.0)/(1000.0 - 500.0) * (new_val-500.0))+1.0;
    else
      new_val = 1.0;

    if (vpp->saturation != new_val) {
      vpp->saturation = new_val;
      vpp->flags |= GST_MFX_POSTPROC_FLAG_SATURATION;
      vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_SATURATION;
    }
  } else if (g_ascii_strcasecmp (channel->label, "BRIGHTNESS") == 0) {
    new_val = (new_val / 10.0);

    if (vpp->brightness != new_val) {
      vpp->brightness = new_val;
      vpp->flags |= GST_MFX_POSTPROC_FLAG_BRIGHTNESS;
      vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_BRIGHTNESS;
    }
  } else if (g_ascii_strcasecmp (channel->label, "CONTRAST") == 0) {
    if (new_val < 500 )
      new_val = ((1 - 0.0)/(500.0 - 0.0) * (new_val));
    else if (new_val > 500 )
      new_val = ((10.0 - 1.0)/(1000.0 - 500.0) * (new_val-500.0))+1.0;
    else
      new_val = 1.0;

    if (vpp->contrast != new_val) {
      vpp->contrast = new_val;
      vpp->flags |= GST_MFX_POSTPROC_FLAG_CONTRAST;
      vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_CONTRAST;
    }
  } else {
    g_warning ("got an unknown channel %s", channel->label);
  }
}

static gint
gst_mfxpostproc_color_balance_get_value (GstColorBalance *cb,
    GstColorBalanceChannel * channel)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (cb);
  gint value = 0;

  g_return_val_if_fail (channel->label != NULL, 0);

  if (g_ascii_strcasecmp (channel->label, "HUE") == 0) {
    value = vpp->hue * 10;
  } else if (g_ascii_strcasecmp (channel->label, "SATURATION") == 0) {
    if (vpp->saturation < 1.0)
      value = vpp->saturation * 500.0;
    else if (vpp->saturation > 1.0)
	    value = (vpp->saturation-1.0) * ((1000.0 - 500.0) / (10 - 1)) + 500;
    else
      value = vpp->saturation;
  } else if (g_ascii_strcasecmp (channel->label, "BRIGHTNESS") == 0) {
      value = vpp->brightness * 10;
  } else if (g_ascii_strcasecmp (channel->label, "CONTRAST") == 0) {
    if (vpp->contrast < 1.0)
      value = vpp->contrast * 500.0;
    else if (vpp->contrast > 1.0)
	    value = (vpp->contrast-1.0) * ((1000.0 - 500.0) / (10 - 1)) + 500;
    else
      value = vpp->contrast;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
  }
  return value;
}

static const GList *
gst_mfxpostproc_color_balance_list_channels (GstColorBalance *cb)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (cb);

  if (!vpp->channels)
    cb_channels_init (vpp);

  return vpp->channels;
}

static void
gst_mfxpostproc_color_balance_iface_init (GstColorBalanceInterface * iface)
{
  iface->list_channels = gst_mfxpostproc_color_balance_list_channels;
  iface->set_value = gst_mfxpostproc_color_balance_set_value;
  iface->get_value = gst_mfxpostproc_color_balance_get_value;
  iface->get_balance_type = gst_mfxpostproc_color_balance_get_type;
}

static void
find_best_size (GstMfxPostproc * vpp, GstVideoInfo * vip,
    guint * width_ptr, guint * height_ptr)
{
  guint width, height;

  width = GST_VIDEO_INFO_WIDTH (vip);
  height = GST_VIDEO_INFO_HEIGHT (vip);
  if (vpp->width && vpp->height) {
    width = vpp->width;
    height = vpp->height;
  } else if (vpp->keep_aspect) {
    const gdouble ratio = (gdouble) width / height;
    if (vpp->width) {
      width = vpp->width;
      height = vpp->width / ratio;
    } else if (vpp->height) {
      height = vpp->height;
      width = vpp->height * ratio;
    }
  } else if (vpp->width)
    width = vpp->width;
  else if (vpp->height)
    height = vpp->height;

  if (GST_MFX_ROTATION_90 == vpp->angle ||
      GST_MFX_ROTATION_270 == vpp->angle) {
    width = width ^ height;
    height = width ^ height;
    width = width ^ height;
  }
  *width_ptr = width;
  *height_ptr = height;
}

static void
gst_mfxpostproc_destroy (GstMfxPostproc * vpp)
{
  gst_mfx_filter_replace (&vpp->filter, NULL);
  cb_channels_finalize (vpp);
  gst_caps_replace (&vpp->allowed_sinkpad_caps, NULL);
  gst_caps_replace (&vpp->allowed_srcpad_caps, NULL);
}

static gboolean
gst_mfxpostproc_ensure_filter (GstMfxPostproc * vpp)
{
  GstMfxPluginBase *plugin = GST_MFX_PLUGIN_BASE (vpp);
  gboolean sinkpad_has_raw_caps =
      !gst_caps_has_mfx_surface (plugin->sinkpad_caps);
  gboolean srcpad_has_raw_caps =
      gst_mfx_query_peer_has_raw_caps (GST_MFX_PLUGIN_BASE_SRC_PAD (vpp));

  if (vpp->filter)
    return TRUE;

  if (!gst_mfx_plugin_base_ensure_aggregator (plugin))
    return FALSE;

  if (!plugin->sinkpad_has_dmabuf) {
    GstMfxTask *task =
        gst_mfx_task_aggregator_get_current_task (plugin->aggregator);

    if (task) {
      if (sinkpad_has_raw_caps || srcpad_has_raw_caps)
        plugin->sinkpad_caps_is_raw = TRUE;
      else
        plugin->sinkpad_caps_is_raw = !gst_mfx_task_has_video_memory (task);
      gst_mfx_task_unref (task);
    }
  }

  /* If sinkpad caps indicate video memory input, srcpad caps should also
   * indicate video memory output for correct vid-in / vid-out configuration */
  if (!plugin->sinkpad_caps_is_raw && srcpad_has_raw_caps)
    srcpad_has_raw_caps = FALSE;

  /* Prevent pass-through mode if input / output memory types don't match */
  if (plugin->sinkpad_caps_is_raw != srcpad_has_raw_caps)
    vpp->flags |= GST_MFX_POSTPROC_FLAG_CUSTOM;

  plugin->srcpad_caps_is_raw = srcpad_has_raw_caps;

  vpp->filter = gst_mfx_filter_new (plugin->aggregator,
      plugin->sinkpad_caps_is_raw, srcpad_has_raw_caps);
  if (!vpp->filter)
    return FALSE;

  if (plugin->srcpad_caps_is_raw)
    gst_mfx_task_aggregator_update_peer_memtypes (plugin->aggregator, TRUE);

  return TRUE;
}

static gboolean
video_info_changed (GstVideoInfo * old_vip, GstVideoInfo * new_vip)
{
  if (GST_VIDEO_INFO_FORMAT (old_vip) != GST_VIDEO_INFO_FORMAT (new_vip))
    return TRUE;
  if (GST_VIDEO_INFO_WIDTH (old_vip) != GST_VIDEO_INFO_WIDTH (new_vip))
    return TRUE;
  if (GST_VIDEO_INFO_HEIGHT (old_vip) != GST_VIDEO_INFO_HEIGHT (new_vip))
    return TRUE;
  return FALSE;
}

gboolean
video_info_update (GstCaps * caps, GstVideoInfo * info,
    gboolean * caps_changed_ptr)
{
  GstVideoInfo vi;

  if (!gst_video_info_from_caps (&vi, caps))
    return FALSE;

  *caps_changed_ptr = FALSE;
  if (video_info_changed (info, &vi)) {
    *caps_changed_ptr = TRUE;
    *info = vi;
  }
  return TRUE;
}

static gboolean
gst_mfxpostproc_update_src_caps (GstMfxPostproc * vpp, GstCaps * caps,
    gboolean * caps_changed_ptr)
{
  GST_INFO_OBJECT (vpp, "new src caps = %" GST_PTR_FORMAT, caps);

  if (!video_info_update (caps, &vpp->srcpad_info, caps_changed_ptr))
    return FALSE;

  if (GST_VIDEO_INFO_FORMAT (&vpp->sinkpad_info) !=
      GST_VIDEO_INFO_FORMAT (&vpp->srcpad_info))
    vpp->flags |= GST_MFX_POSTPROC_FLAG_FORMAT;

  if ((vpp->width || vpp->height) &&
      vpp->width != GST_VIDEO_INFO_WIDTH (&vpp->sinkpad_info) &&
      vpp->height != GST_VIDEO_INFO_HEIGHT (&vpp->sinkpad_info))
    vpp->flags |= GST_MFX_POSTPROC_FLAG_SIZE;

  if (vpp->fps_n && gst_util_fraction_compare(
        GST_VIDEO_INFO_FPS_N (&vpp->srcpad_info),
        GST_VIDEO_INFO_FPS_D (&vpp->srcpad_info),
        GST_VIDEO_INFO_FPS_N (&vpp->sinkpad_info),
        GST_VIDEO_INFO_FPS_D (&vpp->sinkpad_info)))
    vpp->flags |= GST_MFX_POSTPROC_FLAG_FRC;

  return TRUE;
}

static gboolean
gst_mfxpostproc_update_sink_caps (GstMfxPostproc * vpp, GstCaps * caps,
    gboolean * caps_changed_ptr)
{
  GST_INFO_OBJECT (vpp, "new sink caps = %" GST_PTR_FORMAT, caps);

  return video_info_update (caps, &vpp->sinkpad_info, caps_changed_ptr);
}

static GstBuffer *
create_output_buffer (GstMfxPostproc * vpp)
{
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret;

  GstBufferPool *const pool =
      GST_MFX_PLUGIN_BASE (vpp)->srcpad_buffer_pool;

  g_return_val_if_fail (pool != NULL, NULL);

  if (!gst_buffer_pool_set_active (pool, TRUE))
    goto error_activate_pool;

  ret = gst_buffer_pool_acquire_buffer (pool, &outbuf, NULL);
  if (GST_FLOW_OK != ret || !outbuf)
    goto error_create_buffer;

  return outbuf;
  /* Errors */
error_activate_pool:
  {
    GST_ERROR ("failed to activate output video buffer pool");
    return NULL;
  }
error_create_buffer:
  {
    GST_ERROR ("failed to create output video buffer");
    return NULL;
  }
}

static void
gst_mfxpostproc_before_transform (GstBaseTransform * trans,
    GstBuffer * buf)
{
  GstMfxPostproc *vpp = GST_MFXPOSTPROC (trans);

  if (!vpp->flags &&
      (vpp->hue == DEFAULT_HUE ||
       vpp->contrast == DEFAULT_CONTRAST ||
       vpp->saturation == DEFAULT_SATURATION ||
       vpp->brightness == DEFAULT_BRIGHTNESS)) {
    gst_base_transform_set_passthrough (trans, TRUE);
  }
  else {
    gst_base_transform_set_passthrough (trans, FALSE);
  }

  if (vpp->cb_changed) {
    if (vpp->cb_changed & GST_MFX_POSTPROC_FLAG_SATURATION)
      gst_mfx_filter_set_saturation(vpp->filter, vpp->saturation);
    if (vpp->cb_changed & GST_MFX_POSTPROC_FLAG_CONTRAST)
      gst_mfx_filter_set_contrast(vpp->filter, vpp->contrast);
    if (vpp->cb_changed & GST_MFX_POSTPROC_FLAG_HUE)
      gst_mfx_filter_set_hue(vpp->filter, vpp->hue);
    if (vpp->cb_changed & GST_MFX_POSTPROC_FLAG_BRIGHTNESS)
      gst_mfx_filter_set_brightness(vpp->filter, vpp->brightness);
    gst_mfx_filter_reset(vpp->filter);
    vpp->cb_changed = 0;
  }
}

static GstFlowReturn
gst_mfxpostproc_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (trans);
  GstMfxVideoMeta *inbuf_meta, *outbuf_meta;
  GstMfxSurface *surface, *out_surface;
  GstMfxFilterStatus status = GST_MFX_FILTER_STATUS_SUCCESS;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buf = NULL;
  GstMfxRectangle *crop_rect = NULL;
  GstClockTime timestamp;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);

  ret = gst_mfx_plugin_base_get_input_buffer (GST_MFX_PLUGIN_BASE (vpp),
          inbuf, &buf);
  if (GST_FLOW_OK != ret)
    return ret;

  inbuf_meta = gst_buffer_get_mfx_video_meta (buf);
  surface = gst_mfx_video_meta_get_surface (inbuf_meta);
  if (!surface)
    goto error_create_surface;

  do {
    if (vpp->flags & GST_MFX_POSTPROC_FLAG_FRC) {
      if (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE != status)
        gst_buffer_replace (&buf, NULL);
      buf = create_output_buffer (vpp);
      if (!buf)
        goto error_create_buffer;
    }

    status = gst_mfx_filter_process (vpp->filter, surface, &out_surface);
    if (GST_MFX_FILTER_STATUS_SUCCESS != status
        && GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE != status
        && GST_MFX_FILTER_STATUS_ERROR_MORE_DATA != status)
      goto error_process_vpp;

    if (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == status)
      outbuf_meta = gst_buffer_get_mfx_video_meta (buf);
    else
      outbuf_meta = gst_buffer_get_mfx_video_meta (outbuf);

    if (!outbuf_meta)
      goto error_create_meta;

    gst_mfx_video_meta_set_surface (outbuf_meta, out_surface);
    crop_rect = gst_mfx_surface_get_crop_rect (out_surface);
    if (crop_rect) {
      GstVideoCropMeta *const crop_meta =
          gst_buffer_add_video_crop_meta (outbuf);
      if (crop_meta) {
        crop_meta->x = crop_rect->x;
        crop_meta->y = crop_rect->y;
        crop_meta->width = crop_rect->width;
        crop_meta->height = crop_rect->height;
      }
    }

    if (GST_MFX_FILTER_STATUS_ERROR_MORE_DATA == status) {
      gst_buffer_unref (buf);
      return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    if (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == status) {
      GST_BUFFER_TIMESTAMP (buf) = timestamp;
      GST_BUFFER_DURATION (buf) = vpp->field_duration;
      timestamp += vpp->field_duration;
      ret = gst_pad_push (trans->srcpad, buf);
    }
    else {
      if (vpp->flags & GST_MFX_POSTPROC_FLAG_FRC) {
        GST_BUFFER_TIMESTAMP (outbuf) = timestamp;
        GST_BUFFER_DURATION (outbuf) = vpp->field_duration;
      }
      else {
        gst_buffer_copy_into (outbuf, inbuf, GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
      }
    }
  } while (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == status
           && GST_FLOW_OK == ret);

#if GST_CHECK_VERSION(1,8,0)
  gst_mfx_plugin_base_export_dma_buffer (GST_MFX_PLUGIN_BASE (vpp), outbuf);
#endif // GST_CHECK_VERSION

  gst_buffer_unref (buf);
  return ret;
  /* ERRORS */
error_create_buffer:
  {
    GST_ERROR ("failed to output buffer");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
error_create_meta:
  {
    GST_ERROR ("failed to create new output buffer meta");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
error_create_surface:
  {
    GST_ERROR ("failed to create surface surface from buffer");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
error_process_vpp:
  {
    GST_ERROR ("failed to apply VPP (error %d)", status);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_mfxpostproc_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  return gst_mfx_plugin_base_propose_allocation (GST_MFX_PLUGIN_BASE (trans),
      query);
}

static gboolean
gst_mfxpostproc_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  return gst_mfx_plugin_base_decide_allocation (GST_MFX_PLUGIN_BASE (trans),
      query);
}

static gboolean
ensure_allowed_sinkpad_caps (GstMfxPostproc * vpp)
{
  if (vpp->allowed_sinkpad_caps)
    return TRUE;

  vpp->allowed_sinkpad_caps =
      gst_static_pad_template_get_caps (&gst_mfxpostproc_sink_factory);
  if (!vpp->allowed_sinkpad_caps) {
    GST_ERROR_OBJECT (vpp, "failed to create MFX sink caps");
    return FALSE;
  }
  return TRUE;
}

static gboolean
ensure_allowed_srcpad_caps (GstMfxPostproc * vpp)
{
  if (vpp->allowed_srcpad_caps)
    return TRUE;

  /* Create initial caps from pad template */
  vpp->allowed_srcpad_caps =
      gst_static_pad_template_get_caps (&gst_mfxpostproc_src_factory);
  if (!vpp->allowed_srcpad_caps) {
    GST_ERROR_OBJECT (vpp, "failed to create MFX src caps");
    return FALSE;
  }
  return TRUE;
}

static GstCaps *
gst_mfxpostproc_transform_caps_impl (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (trans);
  GstVideoInfo vi, peer_vi;
  GstVideoFormat out_format;
  GstCaps *out_caps, *peer_caps;
  GstMfxCapsFeature feature;
  const gchar *feature_str;
  guint width, height, fps_n, fps_d;

  /* Generate the sink pad caps, that could be fixated afterwards */
  if (direction == GST_PAD_SRC) {
    if (!ensure_allowed_sinkpad_caps (vpp))
      return NULL;
    return gst_caps_ref (vpp->allowed_sinkpad_caps);
  }

  /* Generate complete set of src pad caps if non-fixated sink pad
   * caps are provided */
  if (!gst_caps_is_fixed (caps)) {
    if (!ensure_allowed_srcpad_caps (vpp))
      return NULL;
    return gst_caps_ref (vpp->allowed_srcpad_caps);
  }

  /* Generate the expected src pad caps, from the current fixated
   * sink pad caps */
  if (!gst_video_info_from_caps (&vi, caps))
    return NULL;

  if (vpp->deinterlace_mode)
    GST_VIDEO_INFO_INTERLACE_MODE (&vi) = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

  /* Update size from user-specified parameters */
  find_best_size (vpp, &vi, &width, &height);

  /* Update format from user-specified parameters */
  peer_caps = gst_pad_peer_query_caps (GST_BASE_TRANSFORM_SRC_PAD (trans),
      vpp->allowed_srcpad_caps);

  if (gst_caps_is_any (peer_caps) || gst_caps_is_empty (peer_caps))
    return peer_caps;
  if (!gst_caps_is_fixed (peer_caps))
    peer_caps = gst_caps_fixate (peer_caps);

  gst_video_info_from_caps (&peer_vi, peer_caps);
  out_format = GST_VIDEO_INFO_FPS_N (&peer_vi);
  fps_n = GST_VIDEO_INFO_FPS_N (&peer_vi);
  fps_d = GST_VIDEO_INFO_FPS_D (&peer_vi);

  /* Update width and height from the caps */
  if (GST_VIDEO_INFO_HEIGHT (&peer_vi) != 1 &&
      GST_VIDEO_INFO_WIDTH (&peer_vi) != 1)
    find_best_size(vpp, &peer_vi, &width, &height);

  if (vpp->format != DEFAULT_FORMAT)
    out_format = vpp->format;

  if (vpp->fps_n) {
    GST_VIDEO_INFO_FPS_N (&vi) = vpp->fps_n;
    GST_VIDEO_INFO_FPS_D (&vi) = vpp->fps_d;
    vpp->field_duration = gst_util_uint64_scale (GST_SECOND,
        vpp->fps_d, vpp->fps_n);
    if (DEFAULT_FRC_ALG == vpp->alg)
      vpp->alg = GST_MFX_FRC_PRESERVE_TIMESTAMP;
  }

  if (peer_caps)
    gst_caps_unref (peer_caps);

  feature =
      gst_mfx_find_preferred_caps_feature (GST_BASE_TRANSFORM_SRC_PAD (trans),
        &out_format);
  gst_video_info_change_format (&vi, out_format, width, height);

  out_caps = gst_video_info_to_caps (&vi);
  if (!out_caps)
    return NULL;


  if (feature) {
    feature_str = gst_mfx_caps_feature_to_string (feature);
    if (feature_str)
      gst_caps_set_features (out_caps, 0,
          gst_caps_features_new (feature_str, NULL));
  }

  if (vpp->format != out_format)
    vpp->format = out_format;

  return out_caps;
}

static GstCaps *
gst_mfxpostproc_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *out_caps;

  caps = gst_mfxpostproc_transform_caps_impl (trans, direction, caps);
  if (caps && filter) {
    out_caps = gst_caps_intersect_full (caps, filter, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);

    return out_caps;
  }

  return caps;
}

static gboolean
gst_mfxpostproc_create (GstMfxPostproc * vpp)
{
  if (!gst_mfxpostproc_ensure_filter (vpp))
    return FALSE;

  gst_mfx_filter_set_frame_info_from_gst_video_info (vpp->filter,
      &vpp->sinkpad_info);

  if (vpp->async_depth)
    gst_mfx_filter_set_async_depth (vpp->filter, vpp->async_depth);

  gst_mfx_filter_set_size (vpp->filter,
    GST_VIDEO_INFO_WIDTH (&vpp->srcpad_info),
    GST_VIDEO_INFO_HEIGHT (&vpp->srcpad_info));

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_FORMAT)
    gst_mfx_filter_set_format (vpp->filter,
      gst_video_format_to_mfx_fourcc (vpp->format));

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_DENOISE)
    gst_mfx_filter_set_denoising_level (vpp->filter, vpp->denoise_level);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_DETAIL)
    gst_mfx_filter_set_detail_level (vpp->filter, vpp->detail_level);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_HUE)
    gst_mfx_filter_set_hue (vpp->filter, vpp->hue);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_SATURATION)
    gst_mfx_filter_set_saturation (vpp->filter, vpp->saturation);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_BRIGHTNESS)
    gst_mfx_filter_set_brightness (vpp->filter, vpp->brightness);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_CONTRAST)
    gst_mfx_filter_set_contrast (vpp->filter, vpp->contrast);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_ROTATION)
    gst_mfx_filter_set_rotation (vpp->filter, vpp->angle);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_DEINTERLACING)
    gst_mfx_filter_set_deinterlace_mode (vpp->filter, vpp->deinterlace_mode);

  if (vpp->flags & GST_MFX_POSTPROC_FLAG_FRC) {
    gst_mfx_filter_set_frc_algorithm (vpp->filter, vpp->alg);
    gst_mfx_filter_set_framerate (vpp->filter, vpp->fps_n, vpp->fps_d);
  }

  return  gst_mfx_filter_prepare (vpp->filter);
}

static gboolean
gst_mfxpostproc_set_caps (GstBaseTransform * trans, GstCaps * caps,
    GstCaps * out_caps)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (trans);
  gboolean caps_changed = FALSE;

  if (!gst_mfxpostproc_update_sink_caps (vpp, caps, &caps_changed))
    return FALSE;

  if (!gst_mfxpostproc_update_src_caps (vpp, out_caps, &caps_changed))
    return FALSE;

  if (caps_changed) {
    gst_mfxpostproc_destroy (vpp);

    if (!gst_mfx_plugin_base_set_caps (GST_MFX_PLUGIN_BASE (vpp),
            caps, out_caps))
      return FALSE;

    if (!gst_mfxpostproc_create (vpp))
      return FALSE;
  }
  return TRUE;
}

static gboolean
gst_mfxpostproc_query (GstBaseTransform * trans, GstPadDirection direction,
    GstQuery * query)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (trans);

  if (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT) {
    if (gst_mfx_handle_context_query (query,
          GST_MFX_PLUGIN_BASE_AGGREGATOR (vpp))) {
      GST_DEBUG_OBJECT (vpp, "sharing tasks %p",
        GST_MFX_PLUGIN_BASE_AGGREGATOR (vpp));
      return TRUE;
    }
  }

  return
      GST_BASE_TRANSFORM_CLASS (gst_mfxpostproc_parent_class)->query (trans,
        direction, query);
}

static gboolean
gst_mfxpostproc_stop (GstBaseTransform * trans)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (trans);

  gst_video_info_init (&vpp->sinkpad_info);
  gst_video_info_init (&vpp->srcpad_info);

  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (vpp), FALSE);
  gst_mfxpostproc_destroy (vpp);
  gst_mfx_plugin_base_close (GST_MFX_PLUGIN_BASE (vpp));

  return TRUE;
}

static void
gst_mfxpostproc_finalize (GObject * object)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (object);

  gst_mfx_plugin_base_finalize (GST_MFX_PLUGIN_BASE (vpp));
  G_OBJECT_CLASS (gst_mfxpostproc_parent_class)->finalize (object);
}


static void
gst_mfxpostproc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (object);

  switch (prop_id) {
    case PROP_ASYNC_DEPTH:
      vpp->async_depth = g_value_get_uint (value);
      break;
    case PROP_FORMAT:
      vpp->format = g_value_get_enum (value);
      break;
    case PROP_WIDTH:
      vpp->width = g_value_get_uint (value);
      break;
    case PROP_HEIGHT:
      vpp->height = g_value_get_uint (value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      vpp->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_DEINTERLACE_MODE:
      vpp->deinterlace_mode = g_value_get_enum (value);
      vpp->flags |= GST_MFX_POSTPROC_FLAG_DEINTERLACING;
      break;
    case PROP_DENOISE:
      vpp->denoise_level = g_value_get_uint (value);
      vpp->flags |= GST_MFX_POSTPROC_FLAG_DENOISE;
      break;
    case PROP_DETAIL:
      vpp->detail_level = g_value_get_uint (value);
      vpp->flags |= GST_MFX_POSTPROC_FLAG_DETAIL;
      break;
    case PROP_HUE:
      if (vpp->hue != g_value_get_float (value)) {
        vpp->hue = g_value_get_float (value);
        vpp->flags |= GST_MFX_POSTPROC_FLAG_HUE;
        vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_HUE;
      }
      break;
    case PROP_SATURATION:
      if (vpp->saturation != g_value_get_float (value)) {
        vpp->saturation = g_value_get_float (value);
        vpp->flags |= GST_MFX_POSTPROC_FLAG_SATURATION;
        vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_SATURATION;
      }
      break;
    case PROP_BRIGHTNESS:
      if (vpp->brightness != g_value_get_float (value)) {
        vpp->brightness = g_value_get_float (value);
        vpp->flags |= GST_MFX_POSTPROC_FLAG_BRIGHTNESS;
        vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_BRIGHTNESS;
      }
      break;
    case PROP_CONTRAST:
      if (vpp->contrast != g_value_get_float (value)) {
        vpp->contrast = g_value_get_float (value);
        vpp->flags |= GST_MFX_POSTPROC_FLAG_CONTRAST;
        vpp->cb_changed |= GST_MFX_POSTPROC_FLAG_CONTRAST;
      }
      break;
    case PROP_ROTATION:
      vpp->angle = g_value_get_enum (value);
      vpp->flags |= GST_MFX_POSTPROC_FLAG_ROTATION;
      break;
    case PROP_FRAMERATE:
      vpp->fps_n = gst_value_get_fraction_numerator (value);
      vpp->fps_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_FRC_ALGORITHM:
      vpp->alg = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_mfxpostproc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstMfxPostproc *const vpp = GST_MFXPOSTPROC (object);

  switch (prop_id) {
    case PROP_ASYNC_DEPTH:
      g_value_set_uint (value, vpp->async_depth);
      break;
    case PROP_FORMAT:
      g_value_set_enum (value, vpp->format);
      break;
    case PROP_WIDTH:
      g_value_set_uint (value, vpp->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, vpp->height);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, vpp->keep_aspect);
      break;
    case PROP_DEINTERLACE_MODE:
      g_value_set_enum (value, vpp->deinterlace_mode);
      break;
    case PROP_DENOISE:
      g_value_set_uint (value, vpp->denoise_level);
      break;
    case PROP_DETAIL:
      g_value_set_uint (value, vpp->detail_level);
      break;
    case PROP_HUE:
      g_value_set_float (value, vpp->hue);
      break;
    case PROP_SATURATION:
      g_value_set_float (value, vpp->saturation);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_float (value, vpp->brightness);
      break;
    case PROP_CONTRAST:
      g_value_set_float (value, vpp->contrast);
      break;
    case PROP_ROTATION:
      g_value_set_enum (value, vpp->angle);
      break;
    case PROP_FRAMERATE:
      if (vpp->fps_n && vpp->fps_d)
        gst_value_set_fraction (value, vpp->fps_n, vpp->fps_d);
      break;
    case PROP_FRC_ALGORITHM:
      g_value_set_enum (value, vpp->alg);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_mfxpostproc_class_init (GstMfxPostprocClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  GstElementClass *const element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *const trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstPadTemplate *pad_template;

  GST_DEBUG_CATEGORY_INIT (gst_debug_mfxpostproc,
      GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

  gst_mfx_plugin_base_class_init (GST_MFX_PLUGIN_BASE_CLASS (klass));

  object_class->finalize = gst_mfxpostproc_finalize;
  object_class->set_property = gst_mfxpostproc_set_property;
  object_class->get_property = gst_mfxpostproc_get_property;
  trans_class->transform_caps = gst_mfxpostproc_transform_caps;
  trans_class->transform = gst_mfxpostproc_transform;
  trans_class->set_caps = gst_mfxpostproc_set_caps;
  trans_class->stop = gst_mfxpostproc_stop;
  trans_class->query = gst_mfxpostproc_query;
  trans_class->propose_allocation = gst_mfxpostproc_propose_allocation;
  trans_class->decide_allocation = gst_mfxpostproc_decide_allocation;
  trans_class->before_transform = gst_mfxpostproc_before_transform;

  gst_element_class_set_static_metadata (element_class,
      "MFX video postprocessing",
      "Filter/Converter/Video;Filter/Converter/Video/Scaler;"
      "Filter/Effect/Video;Filter/Effect/Video/Deinterlace",
      GST_PLUGIN_DESC, "Ishmael Sameen <ishmael.visayana.sameen@intel.com>");

  /* sink pad */
  pad_template = gst_static_pad_template_get (&gst_mfxpostproc_sink_factory);
  gst_element_class_add_pad_template (element_class, pad_template);

  /* src pad */
  pad_template = gst_static_pad_template_get (&gst_mfxpostproc_src_factory);
  gst_element_class_add_pad_template (element_class, pad_template);

  g_object_class_install_property (object_class,
      PROP_ASYNC_DEPTH,
      g_param_spec_uint ("async-depth", "Asynchronous Depth",
          "Number of async operations before explicit sync",
          0, 20, DEFAULT_ASYNC_DEPTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:deinterlace-mode
   *
   * This selects whether the deinterlacing should always be applied
   * or if they should only be applied on content that has the
   * "interlaced" flag on the caps.
   */
  g_object_class_install_property
      (object_class,
      PROP_DEINTERLACE_MODE,
      g_param_spec_enum ("deinterlace-mode",
          "Deinterlace mode",
          "Deinterlace mode to use",
          GST_MFX_TYPE_DEINTERLACE_MODE,
          DEFAULT_DEINTERLACE_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:width
   *
   * The forced output width in pixels. If set to zero, the width is
   * calculated from the height if aspect ration is preserved, or
   * inherited from the sink caps width
   */
  g_object_class_install_property
      (object_class,
      PROP_WIDTH,
      g_param_spec_uint ("width",
          "Width",
          "Forced output width",
          0, 8192, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:height
   *
   * The forced output height in pixels. If set to zero, the height is
   * calculated from the width if aspect ration is preserved, or
   * inherited from the sink caps height
   */
  g_object_class_install_property
      (object_class,
      PROP_HEIGHT,
      g_param_spec_uint ("height",
          "Height",
          "Forced output height",
          0, 8192, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:force-aspect-ratio
   *
   * When enabled, scaling respects video aspect ratio; when disabled,
   * the video is distorted to fit the width and height properties.
   */
  g_object_class_install_property
      (object_class,
      PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:framerate
   *
   * The forced output frame rate specified as a floating-point value
   */
  g_object_class_install_property
      (object_class,
      PROP_FRAMERATE,
      gst_param_spec_fraction ("framerate",
          "Frame rate",
          "Forced output frame rate",
          0, 1, G_MAXINT, 1, 0, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:denoise
   *
   * The level of noise reduction to apply.
   */
  g_object_class_install_property (object_class,
      PROP_DENOISE,
      g_param_spec_uint ("denoise",
          "Denoising Level",
          "The level of denoising to apply",
          0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:detail
   *
   * The level of detail / edge enhancement to apply for positive values.
   */
  g_object_class_install_property (object_class,
      PROP_DETAIL,
      g_param_spec_uint ("detail",
          "Detail Level",
          "The level of detail / edge enhancement to apply",
          0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:hue
   *
   * The color hue, expressed as a float value. Range is -180.0 to
   * 180.0. Default value is 0.0 and represents no modification.
   */
  g_object_class_install_property (object_class,
      PROP_HUE,
      g_param_spec_float ("hue",
          "Hue",
          "The color hue value",
          -180.0, 180.0, 0.0, GST_PARAM_CONTROLLABLE |
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:saturation
   *
   * The color saturation, expressed as a float value. Range is 0.0 to
   * 10.0. Default value is 1.0 and represents no modification.
   */
  g_object_class_install_property (object_class,
      PROP_SATURATION,
      g_param_spec_float ("saturation",
          "Saturation",
          "The color saturation value",
          0.0, 10.0, 1.0, GST_PARAM_CONTROLLABLE |
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:brightness
   *
   * The color brightness, expressed as a float value. Range is -100.0
   * to 100.0. Default value is 0.0 and represents no modification.
   */
  g_object_class_install_property (object_class,
      PROP_BRIGHTNESS,
      g_param_spec_float ("brightness",
          "Brightness",
          "The color brightness value",
          -100.0, 100.0, 0.0, GST_PARAM_CONTROLLABLE |
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxPostproc:contrast
   *
   * The color contrast, expressed as a float value. Range is 0.0 to
   * 10.0. Default value is 1.0 and represents no modification.
   */
  g_object_class_install_property (object_class,
      PROP_CONTRAST,
      g_param_spec_float ("contrast",
          "Contrast",
          "The color contrast value",
          0.0, 10.0, 1.0, GST_PARAM_CONTROLLABLE |
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifndef WITH_MSS_2016
  /**
   * GstMfxPostproc:rotation
   *
   * The rotation angle  for the surface, expressed in GstMfxRotation.
   */
  g_object_class_install_property (object_class,
      PROP_ROTATION,
      g_param_spec_enum ("rotation",
          "Rotation",
          "The rotation angle",
          GST_MFX_TYPE_ROTATION,
          DEFAULT_ROTATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  /**
   * GstMfxPostproc: frc-algorithm
   * The framerate conversion algorithm to convert framerate of the video,
   * expressed in GstMfxFrcAlgorithm.
   */
  g_object_class_install_property (object_class,
      PROP_FRC_ALGORITHM,
      g_param_spec_enum ("frc-algorithm",
          "Algorithm",
          "The algorithm type",
          GST_MFX_TYPE_FRC_ALGORITHM,
          DEFAULT_FRC_ALG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mfxpostproc_init (GstMfxPostproc * vpp)
{
  gst_mfx_plugin_base_init (GST_MFX_PLUGIN_BASE (vpp), GST_CAT_DEFAULT);

  vpp->async_depth = DEFAULT_ASYNC_DEPTH;
  vpp->format = DEFAULT_FORMAT;
  vpp->deinterlace_mode = DEFAULT_DEINTERLACE_MODE;
  vpp->keep_aspect = TRUE;
  vpp->alg = DEFAULT_FRC_ALG;
  vpp->brightness = DEFAULT_BRIGHTNESS;
  vpp->hue = DEFAULT_HUE;
  vpp->saturation = DEFAULT_SATURATION;
  vpp->contrast = DEFAULT_CONTRAST;

  gst_video_info_init (&vpp->sinkpad_info);
  gst_video_info_init (&vpp->srcpad_info);
}
