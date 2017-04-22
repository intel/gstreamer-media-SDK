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

#include "sysdeps.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxencoder_jpeg.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Define default rate control mode ("constant-qp") */
#define DEFAULT_RATECONTROL GST_MFX_RATECONTROL_NONE

/* Supported set of rate control methods, within this implementation */
#define SUPPORTED_RATECONTROLS  (GST_MFX_RATECONTROL_MASK (NONE))

/* ------------------------------------------------------------------------- */
/* --- JPEG Encoder                                                      --- */
/* ------------------------------------------------------------------------- */

#define GST_MFX_ENCODER_JPEG_CAST(encoder) \
  ((GstMfxEncoderJpeg *)(encoder))

struct _GstMfxEncoderJpeg
{
  GstMfxEncoder parent_instance;
};

static GstMfxEncoderStatus
gst_mfx_encoder_jpeg_reconfigure (GstMfxEncoder * base_encoder)
{
  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static gboolean
gst_mfx_encoder_jpeg_init (GstMfxEncoder * base_encoder)
{
  base_encoder->codec = MFX_CODEC_JPEG;

  return TRUE;
}

static void
gst_mfx_encoder_jpeg_finalize (GstMfxEncoder * base_encoder)
{
}

static GstMfxEncoderStatus
gst_mfx_encoder_jpeg_set_property (GstMfxEncoder * base_encoder,
    gint prop_id, const GValue * value)
{
  switch (prop_id) {
    case GST_MFX_ENCODER_JPEG_PROP_QUALITY:
      base_encoder->jpeg_quality = g_value_get_uint (value);
      break;
    default:
      return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
  }
  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

GST_MFX_ENCODER_DEFINE_CLASS_DATA (JPEG);

static inline const GstMfxEncoderClass *
gst_mfx_encoder_jpeg_class (void)
{
  static const GstMfxEncoderClass GstMfxEncoderJpegClass = {
    GST_MFX_ENCODER_CLASS_INIT (Jpeg, jpeg),
    .set_property = gst_mfx_encoder_jpeg_set_property,
  };
  return &GstMfxEncoderJpegClass;
}

GstMfxEncoder *
gst_mfx_encoder_jpeg_new (GstMfxTaskAggregator * aggregator,
    const GstVideoInfo * info, gboolean mapped)
{
  return gst_mfx_encoder_new (gst_mfx_encoder_jpeg_class (),
      aggregator, info, mapped);
}

/**
 * gst_mfx_encoder_jpeg_get_default_properties:
 *
 * Determines the set of JPEG specific encoder properties.
 * The caller owns an extra reference to the resulting array of
 * #GstMfxEncoderPropInfo elements, so it shall be released with
 * g_ptr_array_unref () after usage.
 *
 * Return value: the set of encoder properties for #GstMfxEncoderJpeg,
 *   or %NULL if an error occurred.
 */
GPtrArray *
gst_mfx_encoder_jpeg_get_default_properties (void)
{
  GPtrArray *props = NULL;

  /**
   * GstMfxEncoderJpeg:quality
   *
   * Quality parameter for JPEG encoder
   */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_JPEG_PROP_QUALITY,
      g_param_spec_uint ("quality",
          "Quality", "quality parameter for JPEG encoder", 1, 100, 100,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  return props;
}
