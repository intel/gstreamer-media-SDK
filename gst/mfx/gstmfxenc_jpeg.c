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

#include "gst-libs/mfx/sysdeps.h"
#include "gstmfxenc_jpeg.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideomemory.h"

#include <gst-libs/mfx/gstmfxencoder_jpeg.h>

#define GST_PLUGIN_NAME "mfxjpegenc"
#define GST_PLUGIN_DESC "An MFX-based JPEG encoder"

GST_DEBUG_CATEGORY_STATIC (gst_mfx_jpeg_encode_debug);
#define GST_CAT_DEFAULT gst_mfx_jpeg_encode_debug

#define GST_CODEC_CAPS  "image/jpeg"

static const char gst_mfxenc_jpeg_sink_caps_str[] =
    GST_MFX_MAKE_SURFACE_CAPS "; "
    GST_VIDEO_CAPS_MAKE (GST_MFX_SUPPORTED_INPUT_FORMATS);

static const char gst_mfxenc_jpeg_src_caps_str[] = GST_CODEC_CAPS;

static GstStaticPadTemplate gst_mfxenc_jpeg_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mfxenc_jpeg_sink_caps_str));

static GstStaticPadTemplate gst_mfxenc_jpeg_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mfxenc_jpeg_src_caps_str));


/* jpeg encode */
G_DEFINE_TYPE (GstMfxEncJpeg, gst_mfxenc_jpeg, GST_TYPE_MFXENC);

static void
gst_mfxenc_jpeg_init (GstMfxEncJpeg * encode)
{
  gst_mfxenc_init_properties (GST_MFXENC_CAST (encode));
}

static void
gst_mfxenc_jpeg_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_mfxenc_jpeg_parent_class)->finalize (object);
}

static void
gst_mfxenc_jpeg_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstMfxEncClass *const encode_class = GST_MFXENC_GET_CLASS (object);
  GstMfxEnc *const base_encode = GST_MFXENC_CAST (object);

  switch (prop_id) {
    default:
      if (!encode_class->set_property (base_encode, prop_id, value))
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mfxenc_jpeg_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstMfxEncClass *const encode_class = GST_MFXENC_GET_CLASS (object);
  GstMfxEnc *const base_encode = GST_MFXENC_CAST (object);

  switch (prop_id) {
    default:
      if (!encode_class->get_property (base_encode, prop_id, value))
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_mfxenc_jpeg_get_caps (GstMfxEnc * base_encode)
{
  GstCaps *caps;

  caps = gst_caps_from_string (GST_CODEC_CAPS);

  return caps;
}

static GstMfxEncoder *
gst_mfxenc_jpeg_alloc_encoder (GstMfxEnc * base)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (base);

  if (base->encoder)
    return base->encoder;

  return gst_mfx_encoder_jpeg_new (g_object_new(GST_TYPE_MFX_ENCODER_JPEG, NULL), 
    plugin->aggregator, &plugin->sinkpad_info, plugin->sinkpad_caps_is_raw);
}

static void
gst_mfxenc_jpeg_class_init (GstMfxEncJpegClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  GstElementClass *const element_class = GST_ELEMENT_CLASS (klass);
  GstMfxEncClass *const encode_class = GST_MFXENC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_mfx_jpeg_encode_debug,
      GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

  object_class->finalize = gst_mfxenc_jpeg_finalize;
  object_class->set_property = gst_mfxenc_jpeg_set_property;
  object_class->get_property = gst_mfxenc_jpeg_get_property;

  encode_class->get_properties = gst_mfx_encoder_jpeg_get_default_properties;
  encode_class->get_caps = gst_mfxenc_jpeg_get_caps;
  encode_class->alloc_encoder = gst_mfxenc_jpeg_alloc_encoder;

  gst_element_class_set_static_metadata (element_class,
      "MFX JPEG encoder",
      "Codec/Encoder/Video",
      GST_PLUGIN_DESC, "Ishmael Sameen <ishmael.visayana.sameen@intel.com>");

  /* sink pad */
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_mfxenc_jpeg_sink_factory));

  /* src pad */
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_mfxenc_jpeg_src_factory));

  gst_mfxenc_class_init_properties (encode_class);
}
