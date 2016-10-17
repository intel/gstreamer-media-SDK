/*
 *  Copyright (C) 2016 Intel Corporation
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

#include "gstmfxpluginutil.h"
#include "gstmfxpluginbase.h"
#include "gstmfxsinkbin.h"

#define GST_PLUGIN_NAME "mfxsinkbin"
#define GST_PLUGIN_DESC "A MediaSDK based bin with a postprocessor and a sink"

GST_DEBUG_CATEGORY_STATIC (gst_debug_mfx_sink_bin);
#define GST_CAT_DEFAULT gst_debug_mfx_sink_bin

enum
{
  PROP_0,

  PROP_DISPLAY_TYPE,
  PROP_FULLSCREEN,
  PROP_FORCE_ASPECT_RATIO,
  PROP_NO_FRAME_DROP,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_DEINTERLACE_MODE,
  PROP_DENOISE,
  PROP_DETAIL,
  PROP_HUE,
  PROP_SATURATION,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
#ifndef WITH_MSS
  PROP_ROTATION,
#endif
  N_PROPERTIES
};

static GParamSpec *g_properties[N_PROPERTIES] = { NULL, };

static void
gst_mfx_sink_bin_color_balance_iface_init (GstColorBalanceInterface * iface);

#define DEFAULT_DEINTERLACE_MODE        GST_MFX_DEINTERLACE_MODE_BOB
#define DEFAULT_ROTATION                GST_MFX_ROTATION_0

/* Default templates */
static const char gst_mfx_sink_bin_sink_caps_str[] =
    GST_MFX_MAKE_SURFACE_CAPS "; "
#ifdef WITH_MSS
    GST_VIDEO_CAPS_MAKE ("{ NV12, YV12, I420, YUY2, BGRA, BGRx }");
#else
    GST_VIDEO_CAPS_MAKE ("{ NV12, YV12, I420, UYVY, YUY2, BGRA, BGRx }");
#endif

static GstStaticPadTemplate gst_mfx_sink_bin_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mfx_sink_bin_sink_caps_str));

G_DEFINE_TYPE_WITH_CODE (GstMfxSinkBin, gst_mfx_sink_bin,
    GST_TYPE_BIN,
    GST_MFX_PLUGIN_BASE_INIT_INTERFACES
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_mfx_sink_bin_color_balance_iface_init));

static void
post_missing_element_message (GstMfxSinkBin * mfxsinkbin,
    const char * missing_factory)
{
  GstMessage *msg;

  msg = gst_missing_element_message_new (GST_ELEMENT_CAST (mfxsinkbin),
      missing_factory);

  gst_element_post_message (GST_ELEMENT_CAST (mfxsinkbin), msg);

  GST_ELEMENT_WARNING (mfxsinkbin, CORE, MISSING_PLUGIN,
      ("Missing element '%s' - check your GStreamer installation.",
          missing_factory),  ("rendering might fail"));
}

/* set property method */
static void
gst_mfx_sink_bin_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstMfxSinkBin *mfxsinkbin = GST_MFX_SINK_BIN (object);

  switch (prop_id) {
    case PROP_DISPLAY_TYPE:
      mfxsinkbin->display_type = g_value_get_enum (value);
      g_object_set (G_OBJECT (mfxsinkbin->sink), "display",
          mfxsinkbin->display_type, NULL);
      break;
    case PROP_FULLSCREEN:
      mfxsinkbin->fullscreen = g_value_get_boolean (value);
      g_object_set (G_OBJECT (mfxsinkbin->sink), "fullscreen",
          mfxsinkbin->fullscreen, NULL);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      mfxsinkbin->keep_aspect = g_value_get_boolean (value);
      g_object_set (G_OBJECT (mfxsinkbin->sink), "force-aspect-ratio",
          mfxsinkbin->keep_aspect, NULL);
      break;
    case PROP_NO_FRAME_DROP:
      mfxsinkbin->no_frame_drop = g_value_get_boolean (value);
      g_object_set (G_OBJECT (mfxsinkbin->sink), "no-frame-drop",
          mfxsinkbin->no_frame_drop, NULL);
      break;
   case PROP_WIDTH:
      mfxsinkbin->width = g_value_get_uint (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "width",
          mfxsinkbin->width, NULL);
      break;
    case PROP_HEIGHT:
      mfxsinkbin->height = g_value_get_uint (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "height",
          mfxsinkbin->height, NULL);
      break;
    case PROP_DEINTERLACE_MODE:
      mfxsinkbin->deinterlace_mode = g_value_get_enum (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "deinterlace-mode",
          mfxsinkbin->deinterlace_mode, NULL);
      break;
    case PROP_DENOISE:
      mfxsinkbin->denoise_level = g_value_get_uint (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "denoise",
          mfxsinkbin->denoise_level, NULL);
      break;
    case PROP_DETAIL:
      mfxsinkbin->detail_level = g_value_get_uint (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "detail",
          mfxsinkbin->detail_level, NULL);
      break;
    case PROP_HUE:
      mfxsinkbin->hue = g_value_get_float (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "hue",
          mfxsinkbin->hue, NULL);
      break;
    case PROP_SATURATION:
      mfxsinkbin->saturation = g_value_get_float (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "saturation",
          mfxsinkbin->saturation, NULL);
      break;
    case PROP_BRIGHTNESS:
      mfxsinkbin->brightness = g_value_get_float (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "brightness",
	  mfxsinkbin->brightness, NULL);
      break;
    case PROP_CONTRAST:
      mfxsinkbin->contrast = g_value_get_float (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "contrast",
          mfxsinkbin->contrast, NULL);
      break;
#ifndef WITH_MSS
    case PROP_ROTATION:
      mfxsinkbin->angle = g_value_get_enum (value);
      g_object_set (G_OBJECT (mfxsinkbin->postproc), "rotation",
          mfxsinkbin->angle, NULL);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* get property method */
static void
gst_mfx_sink_bin_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstMfxSinkBin *mfxsinkbin = GST_MFX_SINK_BIN (object);

  switch (prop_id) {
    case PROP_DISPLAY_TYPE:
      g_value_set_enum (value, mfxsinkbin->display_type);
      break;
    case PROP_FULLSCREEN:
      g_value_set_boolean (value, mfxsinkbin->fullscreen);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, mfxsinkbin->keep_aspect);
      break;
    case PROP_NO_FRAME_DROP:
      g_value_set_boolean (value, mfxsinkbin->no_frame_drop);
      break;
   case PROP_WIDTH:
      g_value_set_uint (value, mfxsinkbin->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, mfxsinkbin->height);
      break;
    case PROP_DEINTERLACE_MODE:
      g_value_set_enum (value, mfxsinkbin->deinterlace_mode);
      break;
    case PROP_DENOISE:
      g_value_set_uint (value, mfxsinkbin->denoise_level);
      break;
    case PROP_DETAIL:
      g_value_set_uint (value, mfxsinkbin->detail_level);
      break;
    case PROP_HUE:
      g_value_set_float (value, mfxsinkbin->hue);
      break;
    case PROP_SATURATION:
      g_value_set_float (value, mfxsinkbin->saturation);
      break;
    case PROP_BRIGHTNESS:
      g_value_set_float (value, mfxsinkbin->brightness);
      break;
    case PROP_CONTRAST:
      g_value_set_float (value, mfxsinkbin->contrast);
      break;
#ifndef WITH_MSS
    case PROP_ROTATION:
      g_value_set_enum (value, mfxsinkbin->angle);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mfx_sink_bin_class_init (GstMfxSinkBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_mfx_sink_bin_set_property;
  gobject_class->get_property = gst_mfx_sink_bin_get_property;

  gst_element_class_set_static_metadata (element_class,
      "MFX Sink Bin",
      "Sink/Video",
      GST_PLUGIN_DESC,
      "Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>");

  /* Property */

  /**
   * GstMfxSinkBin:display:
   *
   * The type of display to use.
   */
  g_properties[PROP_DISPLAY_TYPE] =
      g_param_spec_enum ("display",
      "display type",
      "display type to use",
      GST_MFX_TYPE_DISPLAY_TYPE,
      GST_MFX_DISPLAY_TYPE_ANY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:fullscreen:
   *
   * Selects whether fullscreen mode is enabled or not.
   */
  g_properties[PROP_FULLSCREEN] =
      g_param_spec_boolean ("fullscreen",
      "Fullscreen",
      "Requests window in fullscreen state",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:force-aspect-ratio:
   *
   * When enabled, scaling respects video aspect ratio; when disabled,
   * the video is distorted to fit the window.
   */
  g_properties[PROP_FORCE_ASPECT_RATIO] =
      g_param_spec_boolean ("force-aspect-ratio",
      "Force aspect ratio",
      "When enabled, scaling will respect original aspect ratio",
      TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:no-frame-drop:
   *
   * When enabled, all decoded frames arriving at the sink will be rendered
   * regardless of its lateness. This option helps to deal with slow initial
   * render times and possible frame drops when rendering the first few frames.
   */
  g_properties[PROP_NO_FRAME_DROP] =
      g_param_spec_boolean ("no-frame-drop",
      "No frame drop",
      "When enabled, no frame will dropped",
      TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:deinterlace-mode:
   *
   * This selects whether the deinterlacing should always be applied
   * or if they should only be applied on content that has the
   * "interlaced" flag on the caps.
   */
  g_properties[PROP_DEINTERLACE_MODE] =
      g_param_spec_enum ("deinterlace-mode",
          "Deinterlace mode",
          "Deinterlace mode to use",
          GST_MFX_TYPE_DEINTERLACE_MODE,
          GST_MFX_DEINTERLACE_MODE_BOB,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:width:
   *
   * The forced output width in pixels. If set to zero, the width is
   * calculated from the height if aspect ration is preserved, or
   * inherited from the sink caps width
   */
  g_properties[PROP_WIDTH] =
      g_param_spec_uint ("width",
          "Width",
          "Forced output width",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:height:
   *
   * The forced output height in pixels. If set to zero, the height is
   * calculated from the width if aspect ration is preserved, or
   * inherited from the sink caps height
   */
  g_properties[PROP_HEIGHT] =
      g_param_spec_uint ("height",
          "Height",
          "Forced output height",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:denoise:
   *
   * The level of noise reduction to apply.
   */
  g_properties[PROP_DENOISE] =
      g_param_spec_uint ("denoise",
          "Denoising Level",
          "The level of denoising to apply",
          0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:detail:
   *
   * The level of detail / edge enhancement to apply for positive values.
   */
  g_properties[PROP_DETAIL] =
      g_param_spec_uint ("detail",
          "Detail Level",
          "The level of detail / edge enhancement to apply",
          0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:hue:
   *
   * The color hue, expressed as a float value. Range is -180.0 to
   * 180.0. Default value is 0.0 and represents no modification.
   */
  g_properties[PROP_HUE] =
      g_param_spec_float ("hue",
          "Hue",
          "The color hue value",
          -180.0, 180.0, 0.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:saturation:
   *
   * The color saturation, expressed as a float value. Range is 0.0 to
   * 10.0. Default value is 1.0 and represents no modification.
   */
  g_properties[PROP_SATURATION] =
      g_param_spec_float ("saturation",
          "Saturation",
          "The color saturation value",
          0.0, 10.0, 1.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:brightness:
   *
   * The color brightness, expressed as a float value. Range is -100.0
   * to 100.0. Default value is 0.0 and represents no modification.
   */
  g_properties[PROP_BRIGHTNESS] =
      g_param_spec_float ("brightness",
          "Brightness",
          "The color brightness value",
          -100.0, 100.0, 0.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:contrast:
   *
   * The color contrast, expressed as a float value. Range is 0.0 to
   * 10.0. Default value is 1.0 and represents no modification.
   */
  g_properties[PROP_CONTRAST] =
      g_param_spec_float ("contrast",
          "Contrast",
          "The color contrast value",
          0.0, 10.0, 1.0,
          GST_PARAM_CONTROLLABLE |  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSinkBin:rotation:
   *
   * The rotation angle  for the surface, expressed in GstMfxRotation.
   */
#ifndef WITH_MSS
  g_properties[PROP_ROTATION] =
      g_param_spec_enum ("rotation",
          "Rotation",
          "The rotation angle",
          GST_MFX_TYPE_ROTATION,
          DEFAULT_ROTATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
#endif


  g_object_class_install_properties (gobject_class, N_PROPERTIES, g_properties);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_mfx_sink_bin_sink_factory));

  GST_DEBUG_CATEGORY_INIT (gst_debug_mfx_sink_bin,
      GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);
}

static gboolean
gst_mfx_sink_bin_configure (GstMfxSinkBin * mfxsinkbin)
{
  const gchar *missing_factory = NULL;
  GstPad *pad, *ghostpad;

  /* create the VPP */
  mfxsinkbin->postproc = gst_element_factory_make ("mfxvpp", NULL);
  if (!mfxsinkbin->postproc) {
    missing_factory = "mfxvpp";
    goto error_element_missing;
  }

  /* create the sink */
  mfxsinkbin->sink = gst_element_factory_make ("mfxsink", NULL);
  if(!mfxsinkbin->sink)  {
    missing_factory = "mfxsink";
    goto error_element_missing;
  }

  gst_bin_add_many (GST_BIN (mfxsinkbin), mfxsinkbin->postproc,
      mfxsinkbin->sink, NULL);

  if (!gst_element_link_many (mfxsinkbin->postproc,
      mfxsinkbin->sink, NULL))
    goto error_link_pad;

  /* ghost pad sink */
  pad = gst_element_get_static_pad (GST_ELEMENT (mfxsinkbin->postproc),
      "sink");

  ghostpad = gst_ghost_pad_new_from_template ("sink", pad,
      GST_PAD_PAD_TEMPLATE (pad));

  gst_object_unref (pad);
  if (!gst_element_add_pad (GST_ELEMENT (mfxsinkbin), ghostpad))
      goto error_adding_pad;

  gst_mfx_object_add_control_binding_proxy (
      GST_OBJECT (mfxsinkbin->postproc),
      GST_OBJECT (mfxsinkbin),
      "contrast");
  gst_mfx_object_add_control_binding_proxy (
      GST_OBJECT (mfxsinkbin->postproc),
      GST_OBJECT (mfxsinkbin),
      "brightness");
  gst_mfx_object_add_control_binding_proxy (
      GST_OBJECT (mfxsinkbin->postproc),
      GST_OBJECT (mfxsinkbin),
      "saturation");
  gst_mfx_object_add_control_binding_proxy (
      GST_OBJECT (mfxsinkbin->postproc),
      GST_OBJECT (mfxsinkbin),
      "hue");

  return TRUE;

error_element_missing:
  {
    post_missing_element_message (mfxsinkbin, missing_factory);
    return FALSE;
  }
error_link_pad:
  {
    GST_ELEMENT_ERROR (mfxsinkbin, CORE, PAD, (NULL),
        ("Failed to configure the mfxsinkplugin."));
    return FALSE;
  }
error_adding_pad:
  {
    GST_ELEMENT_ERROR (mfxsinkbin, CORE, PAD, (NULL),
        ("Failed to add pads."));
    return FALSE;
  }
}

static void
gst_mfx_sink_bin_init (GstMfxSinkBin * mfxsinkbin)
{
  gst_mfx_sink_bin_configure (mfxsinkbin);
}

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
  {CB_HUE, "MFX_HUE", -180.0, 180.0},
  {CB_SATURATION, "MFX_SATURATION", 0.0, 10.0},
  {CB_BRIGHTNESS, "MFX_BRIGHTNESS", -100.0, 100.0},
  {CB_CONTRAST, "MFX_CONTRAST", 0.0, 10.0}
};

static GstColorBalanceType
gst_mfx_sink_bin_color_balance_get_type (GstColorBalance *cb)
{
  GstMfxSinkBin *const sinkbin = GST_MFX_SINK_BIN (cb);
  GstColorBalance *cb_element = NULL;
  GstColorBalanceType type = 0;

  cb_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (sinkbin),
          GST_TYPE_COLOR_BALANCE));

  if (cb_element) {
    type = gst_color_balance_get_balance_type (cb_element);
    gst_object_unref (cb_element);
  }
  return type;
}

static void 
gst_mfx_sink_bin_color_balance_set_value (GstColorBalance * cb,
    GstColorBalanceChannel * channel, gint value)
{
  GstMfxSinkBin *const sinkbin = GST_MFX_SINK_BIN (cb);
  GstColorBalance *cb_element = NULL;

  cb_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (sinkbin),
          GST_TYPE_COLOR_BALANCE));

  if (cb_element) {
    gst_color_balance_set_value (cb_element, channel, value);
    gst_object_unref (cb_element);
  }
}


static gint
gst_mfx_sink_bin_color_balance_get_value (GstColorBalance *cb,
    GstColorBalanceChannel * channel)
{
  GstMfxSinkBin *const sinkbin = GST_MFX_SINK_BIN (cb);
  GstColorBalance *cb_element = NULL;
  gint value = 0;

  cb_element = 
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (sinkbin),
          GST_TYPE_COLOR_BALANCE));

  if (cb_element) {
    value = gst_color_balance_get_value (cb_element, channel);
    gst_object_unref (cb_element);
  }
  return value;
}

static const GList *
gst_mfx_sink_bin_color_balance_list_channels (GstColorBalance *cb)
{
  GstMfxSinkBin *const sinkbin = GST_MFX_SINK_BIN (cb);
  GstColorBalance *cb_element = NULL;
  const GList *list = NULL;

  cb_element =
      GST_COLOR_BALANCE (gst_bin_get_by_interface (GST_BIN (sinkbin),
          GST_TYPE_COLOR_BALANCE));

  if (cb_element) {
    list = gst_color_balance_list_channels (cb_element);
    gst_object_unref (cb_element);
  }
  return list;
}

static void
gst_mfx_sink_bin_color_balance_iface_init (GstColorBalanceInterface * iface)
{
  iface->list_channels = gst_mfx_sink_bin_color_balance_list_channels;
  iface->set_value = gst_mfx_sink_bin_color_balance_set_value;
  iface->get_value = gst_mfx_sink_bin_color_balance_get_value;
  iface->get_balance_type = gst_mfx_sink_bin_color_balance_get_type;
}
