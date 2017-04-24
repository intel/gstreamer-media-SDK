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

static void
ensure_bitrate (GstMfxEncoderMpeg2 * encoder)
{
  GstMfxEncoder *const base_encoder = GST_MFX_ENCODER_CAST (encoder);

  /* Default compression: 64 bits per macroblock */
  switch (GST_MFX_ENCODER_RATE_CONTROL (encoder)) {
    case GST_MFX_RATECONTROL_CBR:
    case GST_MFX_RATECONTROL_VBR:
    case GST_MFX_RATECONTROL_AVBR:
      if (!base_encoder->bitrate)
        base_encoder->bitrate = GST_MFX_ENCODER_WIDTH (encoder) *
            GST_MFX_ENCODER_HEIGHT (encoder) *
            GST_MFX_ENCODER_FPS_N (encoder) /
            GST_MFX_ENCODER_FPS_D (encoder) / 4 / 1000;
      break;
    default:
      base_encoder->bitrate = 0;
      break;
  }
}

static GstMfxEncoderStatus
gst_mfx_encoder_mpeg2_reconfigure (GstMfxEncoder * base_encoder)
{
  GstMfxEncoderMpeg2 *const encoder = GST_MFX_ENCODER_MPEG2_CAST (base_encoder);

  /* Ensure bitrate if not set */
  ensure_bitrate (encoder);

  GST_DEBUG ("resolution: %dx%d", GST_MFX_ENCODER_WIDTH (encoder),
      GST_MFX_ENCODER_HEIGHT (encoder));

  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static gboolean
gst_mfx_encoder_mpeg2_init (GstMfxEncoder * base_encoder)
{
  base_encoder->codec = MFX_CODEC_MPEG2;

  return TRUE;
}

static void
gst_mfx_encoder_mpeg2_finalize (GstMfxEncoder * base_encoder)
{
}

GST_MFX_ENCODER_DEFINE_CLASS_DATA (MPEG2);

static inline const GstMfxEncoderClass *
gst_mfx_encoder_mpeg2_class (void)
{
  static const GstMfxEncoderClass GstMfxEncoderMpeg2Class = {
    GST_MFX_ENCODER_CLASS_INIT (Mpeg2, mpeg2),
  };
  return &GstMfxEncoderMpeg2Class;
}

GstMfxEncoder *
gst_mfx_encoder_mpeg2_new (GstMfxTaskAggregator * aggregator,
    const GstVideoInfo * info, gboolean mapped)
{
  return gst_mfx_encoder_new (gst_mfx_encoder_mpeg2_class (),
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
  const GstMfxEncoderClass *const klass = gst_mfx_encoder_mpeg2_class ();
  GPtrArray *props;

  props = gst_mfx_encoder_properties_get_default (klass);
  if (!props)
    return NULL;

  return props;
}
