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
  PROP_FORCE_ASPECT_RATIO,
  PROP_SYNC,
  PROP_MAX_LATENESS,
  PROP_QOS,
  PROP_ASYNC,
  PROP_TS_OFFSET,
  PROP_ENABLE_LAST_SAMPLE,
  PROP_LAST_SAMPLE,
  PROP_BLOCKSIZE,
  PROP_RENDER_DELAY,
  PROP_THROTTLE_TIME,
  PROP_MAX_BITRATE,
  PROP_DISPLAY_TYPE,
  PROP_FULLSCREEN,
  PROP_SHOW_PREROLL_FRAME,
  PROP_NO_FRAME_DROP,
  PROP_FULL_COLOR_RANGE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_DEINTERLACE_MODE,
  PROP_DENOISE,
  PROP_DETAIL,
  PROP_HUE,
  PROP_SATURATION,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
#ifndef WITH_MSS_2016
  PROP_ROTATION,
#endif
  N_PROPERTIES
};

static GParamSpec *g_properties[N_PROPERTIES] = { NULL, };

static void
gst_mfx_sink_bin_color_balance_iface_init (GstColorBalanceInterface * iface);

#define DEFAULT_DEINTERLACE_MODE        GST_MFX_DEINTERLACE_MODE_BOB
#define DEFAULT_ROTATION                GST_MFX_ROTATION_0
#define DEFAULT_SYNC                    TRUE
#define DEFAULT_QOS                     TRUE
#define DEFAULT_ASYNC                   TRUE
#define DEFAULT_TS_OFFSET               0
#define DEFAULT_ENABLE_LAST_SAMPLE      TRUE
#define DEFAULT_BLOCKSIZE               4096
#define DEFAULT_RENDER_DELAY            0
#define DEFAULT_THROTTLE_TIME           0
#define DEFAULT_MAX_BITRATE             0
#define DEFAULT_MAX_LATENESS            20000000

/* Default templates */
static const char gst_mfx_sink_bin_sink_caps_str[] =
    GST_MFX_MAKE_SURFACE_CAPS "; "
    GST_VIDEO_CAPS_MAKE (GST_MFX_SUPPORTED_INPUT_FORMATS);

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
    /* Sink */
    case PROP_DISPLAY_TYPE:
      g_object_set (G_OBJECT (mfxsinkbin->sink),
          pspec->name,
          g_value_get_enum (value),
          NULL);
      break;
    case PROP_MAX_LATENESS:
    case PROP_TS_OFFSET:
      g_object_set (G_OBJECT (mfxsinkbin->sink),
          pspec->name,
          g_value_get_int64 (value),
          NULL);
      break;
    case PROP_BLOCKSIZE:
      g_object_set (G_OBJECT (mfxsinkbin->sink),
          pspec->name,
          g_value_get_uint (value),
          NULL);
      break;
    case PROP_RENDER_DELAY:
    case PROP_THROTTLE_TIME:
    case PROP_MAX_BITRATE:
      g_object_set (G_OBJECT (mfxsinkbin->sink),
          pspec->name,
          g_value_get_uint64 (value),
          NULL);
      break;
    case PROP_ENABLE_LAST_SAMPLE:
    case PROP_ASYNC:
    case PROP_SYNC:
    case PROP_QOS:
    case PROP_FULLSCREEN:
    case PROP_FORCE_ASPECT_RATIO:
    case PROP_NO_FRAME_DROP:
    case PROP_SHOW_PREROLL_FRAME:
    case PROP_FULL_COLOR_RANGE:
      g_object_set (G_OBJECT (mfxsinkbin->sink),
          pspec->name,
          g_value_get_boolean (value),
          NULL);
      break;

    /* VPP */
    case PROP_WIDTH:
    case PROP_HEIGHT:
    case PROP_DENOISE:
    case PROP_DETAIL:
      g_object_set (G_OBJECT (mfxsinkbin->postproc),
          pspec->name,
          g_value_get_uint (value),
          NULL);
      break;

    case PROP_DEINTERLACE_MODE:
#ifndef WITH_MSS_2016
    case PROP_ROTATION:
#endif
      g_object_set (G_OBJECT (mfxsinkbin->postproc),
          pspec->name,
          g_value_get_enum (value),
          NULL);
      break;
    case PROP_HUE:
    case PROP_SATURATION:
    case PROP_BRIGHTNESS:
    case PROP_CONTRAST:
      g_object_set (G_OBJECT (mfxsinkbin->postproc),
          pspec->name,
          g_value_get_float (value),
          NULL);
      break;
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
    /* Sink */
    case PROP_DISPLAY_TYPE:
    case PROP_SYNC:
    case PROP_MAX_LATENESS:
    case PROP_QOS:
    case PROP_ASYNC:
    case PROP_TS_OFFSET:
    case PROP_ENABLE_LAST_SAMPLE:
    case PROP_LAST_SAMPLE:
    case PROP_BLOCKSIZE:
    case PROP_RENDER_DELAY:
    case PROP_THROTTLE_TIME:
    case PROP_MAX_BITRATE:
    case PROP_FULLSCREEN:
    case PROP_FORCE_ASPECT_RATIO:
    case PROP_NO_FRAME_DROP:
    case PROP_SHOW_PREROLL_FRAME:
    case PROP_FULL_COLOR_RANGE:
      if (mfxsinkbin->sink) {
        g_object_get_property (G_OBJECT (mfxsinkbin->sink),
            pspec->name,
            value);
      }
      break;
    /* VPP */
    case PROP_WIDTH:
    case PROP_HEIGHT:
    case PROP_DEINTERLACE_MODE:
    case PROP_DENOISE:
    case PROP_DETAIL:
    case PROP_HUE:
    case PROP_SATURATION:
    case PROP_BRIGHTNESS:
    case PROP_CONTRAST:
#ifndef WITH_MSS_2016
    case PROP_ROTATION:
#endif
      if (mfxsinkbin->postproc) {
        g_object_get_property (G_OBJECT (mfxsinkbin->postproc),
            pspec->name,
            value);
      }
      break;
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
  g_properties[PROP_FORCE_ASPECT_RATIO] =
      g_param_spec_boolean ("force-aspect-ratio",
      "Force aspect ratio",
      "When enabled, scaling will respect original aspect ratio",
      TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /* base sink */
  g_properties[PROP_SYNC] = g_param_spec_boolean ("sync",
      "Sync",
      "Sync on the clock",
      DEFAULT_SYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_MAX_LATENESS] =
      g_param_spec_int64 ("max-lateness",
      "Max Lateness",
      "Maximum number of nanoseconds that a buffer can be late before it "
      "is dropped (-1 unlimited)",
      -1, G_MAXINT64,
      DEFAULT_MAX_LATENESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_QOS] =
      g_param_spec_boolean ("qos",
      "Qos",
      "Generate Quality-of-Service events upstream",
      DEFAULT_QOS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_ASYNC] =
      g_param_spec_boolean ("async", "Async",
      "Go asynchronously to PAUSED",
      DEFAULT_ASYNC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_TS_OFFSET] = g_param_spec_int64 ("ts-offset", "TS Offset",
      "Timestamp offset in nanoseconds",
      G_MININT64, G_MAXINT64,
      DEFAULT_TS_OFFSET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_ENABLE_LAST_SAMPLE] =
  g_param_spec_boolean ("enable-last-sample", "Enable Last Buffer",
      "Enable the last-sample property",
      DEFAULT_ENABLE_LAST_SAMPLE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_LAST_SAMPLE] =
      g_param_spec_boxed ("last-sample", "Last Sample",
      "The last sample received in the sink",
      GST_TYPE_SAMPLE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_BLOCKSIZE] =
      g_param_spec_uint ("blocksize", "Block size",
      "Size in bytes to pull per buffer (0 = default)", 0, G_MAXUINT,
      DEFAULT_BLOCKSIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_RENDER_DELAY] =
      g_param_spec_uint64 ("render-delay", "Render Delay",
      "Additional render delay of the sink in nanoseconds", 0, G_MAXUINT64,
      DEFAULT_RENDER_DELAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_THROTTLE_TIME] =
      g_param_spec_uint64 ("throttle-time", "Throttle time",
      "The time to keep between rendered buffers (0 = disabled)", 0,
      G_MAXUINT64,
      DEFAULT_THROTTLE_TIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_MAX_BITRATE] =
      g_param_spec_uint64 ("max-bitrate", "Max Bitrate",
      "The maximum bits per second to render (0 = disabled)", 0,
      G_MAXUINT64,
      DEFAULT_MAX_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_DISPLAY_TYPE] =
      g_param_spec_enum ("display",
      "display type",
      "display type to use",
      GST_MFX_TYPE_DISPLAY_TYPE,
      GST_MFX_DISPLAY_TYPE_ANY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_FULLSCREEN] =
      g_param_spec_boolean ("fullscreen",
      "Fullscreen",
      "Requests window in fullscreen state",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_NO_FRAME_DROP] =
      g_param_spec_boolean ("no-frame-drop",
      "No frame drop",
      "When enabled, no frame will dropped",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_SHOW_PREROLL_FRAME] =
      g_param_spec_boolean ("show-preroll-frame",
      "Show preroll frame",
      "When enabled, show video frames during preroll.",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_FULL_COLOR_RANGE] =
      g_param_spec_boolean ("full-color-range",
      "Full color range",
      "Decoded frames will be in RGB 0-255",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);


  /* mfxvpp properties */
  g_properties[PROP_DEINTERLACE_MODE] =
      g_param_spec_enum ("deinterlace-mode",
          "Deinterlace mode",
          "Deinterlace mode to use",
          GST_MFX_TYPE_DEINTERLACE_MODE,
          GST_MFX_DEINTERLACE_MODE_BOB,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_WIDTH] =
      g_param_spec_uint ("width",
          "Width",
          "Forced output width",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_HEIGHT] =
      g_param_spec_uint ("height",
          "Height",
          "Forced output height",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_DENOISE] =
      g_param_spec_uint ("denoise",
          "Denoising Level",
          "The level of denoising to apply",
          0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_DETAIL] =
      g_param_spec_uint ("detail",
          "Detail Level",
          "The level of detail / edge enhancement to apply",
          0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_HUE] =
      g_param_spec_float ("hue",
          "Hue",
          "The color hue value",
          -180.0, 180.0, 0.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_SATURATION] =
      g_param_spec_float ("saturation",
          "Saturation",
          "The color saturation value",
          0.0, 10.0, 1.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_BRIGHTNESS] =
      g_param_spec_float ("brightness",
          "Brightness",
          "The color brightness value",
          -100.0, 100.0, 0.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_properties[PROP_CONTRAST] =
      g_param_spec_float ("contrast",
          "Contrast",
          "The color contrast value",
          0.0, 10.0, 1.0,
          GST_PARAM_CONTROLLABLE |  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

#ifndef WITH_MSS_2016
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
  mfxsinkbin->sink = gst_element_factory_make ("mfxsinkelement", NULL);
  if(!mfxsinkbin->sink)  {
    missing_factory = "mfxsinkelement";
    goto error_element_missing;
  }

  gst_bin_add_many (GST_BIN (mfxsinkbin),
    mfxsinkbin->postproc,
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
