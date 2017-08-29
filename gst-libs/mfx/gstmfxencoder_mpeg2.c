/*
 *  Copyright (C) 2012-2014 Intel Corporation
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

#include "sysdeps.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxencoder_mpeg2.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Define default rate control mode ("constant-qp") */
#define DEFAULT_RATECONTROL GST_MFX_RATECONTROL_CQP

/* Supported set of rate control methods, within this implementation */
#define SUPPORTED_RATECONTROLS               \
  (GST_MFX_RATECONTROL_MASK (CQP)   |        \
  GST_MFX_RATECONTROL_MASK (CBR)    |        \
  GST_MFX_RATECONTROL_MASK (VBR)    |        \
  GST_MFX_RATECONTROL_MASK (AVBR)   |        \
  GST_MFX_RATECONTROL_MASK (ICQ))

/* ------------------------------------------------------------------------- */
/* --- MPEG2 Encoder                                                     --- */
/* ------------------------------------------------------------------------- */

#define GST_MFX_ENCODER_MPEG2_CAST(encoder) \
  ((GstMfxEncoderMpeg2 *)(encoder))


struct _GstMfxEncoderMpeg2
{
  GstMfxEncoder parent_instance;
};

G_DEFINE_TYPE (GstMfxEncoderMpeg2, gst_mfx_encoder_mpeg2, GST_TYPE_MFX_ENCODER);

static void
ensure_bitrate (GstMfxEncoderMpeg2 * encoder)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE (encoder);

  /* Default compression: 64 bits per macroblock */
  switch (GST_MFX_ENCODER_RATE_CONTROL (priv)) {
    case GST_MFX_RATECONTROL_CBR:
    case GST_MFX_RATECONTROL_VBR:
    case GST_MFX_RATECONTROL_AVBR:
      if (!priv->bitrate)
        priv->bitrate = GST_MFX_ENCODER_WIDTH (priv) *
            GST_MFX_ENCODER_HEIGHT (priv) *
            GST_MFX_ENCODER_FPS_N (priv) /
            GST_MFX_ENCODER_FPS_D (priv) / 4 / 1000;
      break;
    default:
      priv->bitrate = 0;
      break;
  }
}

static GstMfxEncoderStatus
gst_mfx_encoder_mpeg2_reconfigure (GstMfxEncoder * base_encoder)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE (base_encoder);

  /* Ensure bitrate if not set */
  ensure_bitrate (GST_MFX_ENCODER_MPEG2 (base_encoder));

  GST_DEBUG ("resolution: %dx%d", GST_MFX_ENCODER_WIDTH (priv),
      GST_MFX_ENCODER_HEIGHT (priv));

  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static void
gst_mfx_encoder_mpeg2_init (GstMfxEncoderMpeg2 * encoder)
{
}

static gboolean
gst_mfx_encoder_mpeg2_create (GstMfxEncoder * base_encoder)
{
  GST_MFX_ENCODER_GET_PRIVATE (base_encoder)->profile.codec = MFX_CODEC_MPEG2;
  return TRUE;
}

static void
gst_mfx_encoder_mpeg2_finalize (GstMfxEncoder * base_encoder)
{
}

GstMfxEncoder *
gst_mfx_encoder_mpeg2_new (GstMfxTaskAggregator * aggregator,
    const GstVideoInfo * info, gboolean mapped)
{
  GstMfxEncoderMpeg2 *encoder;

  g_return_val_if_fail (aggregator != NULL, NULL);
  g_return_val_if_fail (info != NULL, NULL);

  encoder = g_object_new (GST_TYPE_MFX_ENCODER_MPEG2, NULL);
  if (!encoder)
    return NULL;

  return gst_mfx_encoder_new (GST_MFX_ENCODER (encoder),
      aggregator, info, mapped);
}

/**
 * gst_mfx_encoder_mpeg2_get_default_properties:
 *
 * Determines the set of common and MPEG2 specific encoder properties.
 * The caller owns an extra reference to the resulting array of
 * #GstMfxEncoderPropInfo elements, so it shall be released with
 * g_ptr_array_unref () after usage.
 *
 * Return value: the set of encoder properties for #GstMfxEncoderMpeg2,
 *   or %NULL if an error occurred.
 */
GPtrArray *
gst_mfx_encoder_mpeg2_get_default_properties (void)
{
  GPtrArray *props;
  {
    GstMfxEncoderClass *const klass =
        g_type_class_ref (GST_TYPE_MFX_ENCODER_MPEG2);
    props = gst_mfx_encoder_properties_get_default (klass);
    g_type_class_unref (klass);
  }
  if (!props)
    return NULL;

  return props;
}

GST_MFX_ENCODER_DEFINE_CLASS_DATA (MPEG2);

static void
gst_mfx_encoder_mpeg2_class_init (GstMfxEncoderMpeg2Class * klass)
{
  GstMfxEncoderClass *const encoder_class = GST_MFX_ENCODER_CLASS (klass);

  encoder_class->class_data = &g_class_data;
  encoder_class->create = gst_mfx_encoder_mpeg2_create;
  encoder_class->finalize = gst_mfx_encoder_mpeg2_finalize;
  encoder_class->reconfigure = gst_mfx_encoder_mpeg2_reconfigure;
  encoder_class->get_default_properties =
      gst_mfx_encoder_mpeg2_get_default_properties;
}
