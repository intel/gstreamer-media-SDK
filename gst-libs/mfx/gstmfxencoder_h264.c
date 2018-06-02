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

#if !GST_CHECK_VERSION(1,15,0)
# include "common/gstbitwriter.h"
#else
# include <gst/base/gstbitwriter.h>
#endif

#include "gstmfxencoder_priv.h"
#include "gstmfxencoder_h264.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Define default rate control mode ("constant-qp") */
#define DEFAULT_RATECONTROL GST_MFX_RATECONTROL_CQP

/* Supported set of rate control methods, within this implementation */
#define SUPPORTED_RATECONTROLS                      \
  (GST_MFX_RATECONTROL_MASK (CQP)     |             \
  GST_MFX_RATECONTROL_MASK (CBR)      |             \
  GST_MFX_RATECONTROL_MASK (VBR)      |             \
  GST_MFX_RATECONTROL_MASK (AVBR)     |             \
  GST_MFX_RATECONTROL_MASK (QVBR)     |             \
  GST_MFX_RATECONTROL_MASK (VCM)      |             \
  GST_MFX_RATECONTROL_MASK (LA_BRC)   |             \
  GST_MFX_RATECONTROL_MASK (LA_HRD)   |             \
  GST_MFX_RATECONTROL_MASK (ICQ)      |             \
  GST_MFX_RATECONTROL_MASK (LA_ICQ))

/* ------------------------------------------------------------------------- */
/* --- H.264 Bitstream Writer                                            --- */
/* ------------------------------------------------------------------------- */

#define WRITE_UINT32(bs, val, nbits) do {                           \
    if (!gst_bit_writer_put_bits_uint32 (bs, val, nbits)) {         \
        GST_WARNING ("failed to write uint32, nbits: %d", nbits);   \
        goto bs_error;                                              \
    }                                                               \
  } while (0)

/* ------------------------------------------------------------------------- */
/* --- H.264 Encoder                                                     --- */
/* ------------------------------------------------------------------------- */


struct _GstMfxEncoderH264
{
  GstMfxEncoder parent_instance;
};

G_DEFINE_TYPE (GstMfxEncoderH264, gst_mfx_encoder_h264, GST_TYPE_MFX_ENCODER);

#define GST_MFX_ENCODER_H264_CAST(encoder) \
  ((GstMfxEncoderH264 *)(encoder))


/* Estimates a good enough bitrate if none was supplied */
static void
ensure_bitrate (GstMfxEncoder * base_encoder)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE (base_encoder);

  /* Default compression: 48 bits per macroblock */
  switch (GST_MFX_ENCODER_RATE_CONTROL (priv)) {
    case GST_MFX_RATECONTROL_CBR:
    case GST_MFX_RATECONTROL_VBR:
    case GST_MFX_RATECONTROL_VCM:
    case GST_MFX_RATECONTROL_AVBR:
    case GST_MFX_RATECONTROL_QVBR:
    case GST_MFX_RATECONTROL_LA_BRC:
    case GST_MFX_RATECONTROL_LA_HRD:
      if (!priv->bitrate) {
        guint mb_width = (GST_MFX_ENCODER_WIDTH (priv) + 15) / 16;
        guint mb_height = (GST_MFX_ENCODER_HEIGHT (priv) + 15) / 16;

        /* According to the literature and testing, CABAC entropy coding
           mode could provide for +10% to +18% improvement in general,
           thus estimating +15% here ; and using adaptive 8x8 transforms
           in I-frames could bring up to +10% improvement. */
        guint bits_per_mb = 48;
        if (!priv->use_cabac)
          bits_per_mb += (bits_per_mb * 15) / 100;

        priv->bitrate =
            mb_width * mb_height * bits_per_mb *
            GST_MFX_ENCODER_FPS_N (priv) / GST_MFX_ENCODER_FPS_D (priv) / 1000;
        GST_INFO ("target bitrate computed to %u kbps", priv->bitrate);
      }
      break;
    default:
      priv->bitrate = 0;
      break;
  }
}

static GstMfxEncoderStatus
gst_mfx_encoder_h264_reconfigure (GstMfxEncoder * base_encoder)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE (base_encoder);

  if (priv->profile.profile == MFX_PROFILE_AVC_BASELINE)
    priv->gop_refdist = 1;

  if (priv->gop_refdist == 1)
    priv->b_strategy = GST_MFX_OPTION_OFF;

  /* Ensure bitrate if not set */
  ensure_bitrate (base_encoder);

  GST_DEBUG ("resolution: %dx%d", GST_MFX_ENCODER_WIDTH (priv),
      GST_MFX_ENCODER_HEIGHT (priv));

  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

/* Generate "codec-data" buffer */
static GstMfxEncoderStatus
gst_mfx_encoder_h264_get_codec_data (GstMfxEncoder * base_encoder,
    GstBuffer ** out_buffer_ptr)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE (base_encoder);
  GstBuffer *buffer;
  mfxStatus sts;
  guint8 *sps_info, *pps_info;
  guint8 sps_data[128], pps_data[128];
  guint sps_size, pps_size;

  const guint configuration_version = 0x01;
  const guint nal_length_size = 4;
  guint8 profile_idc, profile_comp, level_idc;
  GstBitWriter bs;

  mfxExtCodingOptionSPSPPS extradata = {
    .Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS,
    .Header.BufferSz = sizeof (extradata),
    .SPSBuffer = sps_data,.SPSBufSize = sizeof (sps_data),
    .PPSBuffer = pps_data,.PPSBufSize = sizeof (pps_data)
  };

  mfxExtBuffer *ext_buffers[] = {
    (mfxExtBuffer *) & extradata,
  };

  priv->params.ExtParam = ext_buffers;
  priv->params.NumExtParam = 1;

  sts = MFXVideoENCODE_GetVideoParam (priv->session, &priv->params);
  if (sts < 0)
    return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;

  sps_info = &sps_data[4];
  pps_info = &pps_data[4];
  sps_size = extradata.SPSBufSize - 4;
  pps_size = extradata.PPSBufSize - 4;

  /* skip sps_data[0], which is the nal_unit_type */
  profile_idc = sps_info[1];
  profile_comp = sps_info[2];
  level_idc = sps_info[3];

  /* Header */
#if !GST_CHECK_VERSION(1,15,0)
  gst_bit_writer_init (&bs, (sps_size + pps_size + 64) * 8);
#else
  gst_bit_writer_init_with_size (&bs, sps_size + pps_size + 64, FALSE);
#endif
  WRITE_UINT32 (&bs, configuration_version, 8);
  WRITE_UINT32 (&bs, profile_idc, 8);
  WRITE_UINT32 (&bs, profile_comp, 8);
  WRITE_UINT32 (&bs, level_idc, 8);
  WRITE_UINT32 (&bs, 0x3f, 6);  /* 111111 */
  WRITE_UINT32 (&bs, nal_length_size - 1, 2);
  WRITE_UINT32 (&bs, 0x07, 3);  /* 111 */

  /* Write SPS */
  WRITE_UINT32 (&bs, 1, 5);     /* SPS count = 1 */
  g_assert (GST_BIT_WRITER_BIT_SIZE (&bs) % 8 == 0);
  WRITE_UINT32 (&bs, sps_size, 16);
  gst_bit_writer_put_bytes (&bs, sps_info, sps_size);

  /* Write PPS */
  WRITE_UINT32 (&bs, 1, 8);     /* PPS count = 1 */
  WRITE_UINT32 (&bs, pps_size, 16);
  gst_bit_writer_put_bytes (&bs, pps_info, pps_size);

#if !GST_CHECK_VERSION(1,15,0)
  buffer =
      gst_buffer_new_wrapped (GST_BIT_WRITER_DATA (&bs),
      GST_BIT_WRITER_BIT_SIZE (&bs) / 8);
#else
  buffer = gst_bit_writer_reset_and_get_buffer (&bs);
#endif
  if (!buffer)
    goto error_alloc_buffer;
  *out_buffer_ptr = buffer;
#if !GST_CHECK_VERSION(1,15,0)
  gst_bit_writer_clear (&bs, FALSE);
#endif
  return GST_MFX_ENCODER_STATUS_SUCCESS;

  /* ERRORS */
bs_error:
  {
    GST_ERROR ("failed to write codec-data");
#if !GST_CHECK_VERSION(1,15,0)
    gst_bit_writer_clear (&bs, TRUE);
#else
    gst_bit_writer_reset (&bs);
#endif
    return FALSE;
  }
error_alloc_buffer:
  {
    GST_ERROR ("failed to allocate codec-data buffer");
#if !GST_CHECK_VERSION(1,15,0)
    gst_bit_writer_clear (&bs, TRUE);
#else
    gst_bit_writer_reset (&bs);
#endif
    return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
  }
}

static GstMfxEncoderStatus
gst_mfx_encoder_h264_set_property (GstMfxEncoder * base_encoder,
    gint prop_id, const GValue * value)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE (base_encoder);

  switch (prop_id) {
    case GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE:
      priv->max_slice_size = g_value_get_int (value);
      break;
    case GST_MFX_ENCODER_H264_PROP_LA_DEPTH:
      priv->la_depth = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_H264_PROP_CABAC:
      priv->use_cabac = g_value_get_boolean (value);
      break;
    case GST_MFX_ENCODER_H264_PROP_TRELLIS:
      priv->trellis = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_H264_PROP_LOOKAHEAD_DS:
      priv->look_ahead_downsampling = g_value_get_enum (value);
      break;
    default:
      return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
  }
  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

GstMfxEncoder *
gst_mfx_encoder_h264_new (GstMfxTaskAggregator * aggregator,
    const GstVideoInfo * info, gboolean memtype_is_system)
{
  GstMfxEncoderH264 *encoder;

  g_return_val_if_fail (aggregator != NULL, NULL);
  g_return_val_if_fail (info != NULL, NULL);

  encoder = g_object_new (GST_TYPE_MFX_ENCODER_H264, NULL);
  if (!encoder)
    return NULL;

  return gst_mfx_encoder_new (GST_MFX_ENCODER (encoder),
      aggregator, info, memtype_is_system);
}

/**
 * gst_mfx_encoder_h264_get_default_properties:
 *
 * Determines the set of common and H.264 specific encoder properties.
 * The caller owns an extra reference to the resulting array of
 * #GstMfxEncoderPropInfo elements, so it shall be released with
 * g_ptr_array_unref () after usage.
 *
 * Return value: the set of encoder properties for #GstMfxEncoderH264,
 *   or %NULL if an error occurred.
 */
GPtrArray *
gst_mfx_encoder_h264_get_default_properties (void)
{
  GPtrArray *props;
  {
    GstMfxEncoderClass *const klass =
        g_type_class_ref (GST_TYPE_MFX_ENCODER_H264);
    props = gst_mfx_encoder_properties_get_default (klass);
    g_type_class_unref (klass);
  }
  if (!props)
    return NULL;

  /**
   * GstMfxEncoderH264:max-slice-size
   *
   * Maximum encoded slice size in bytes.
   */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE,
      g_param_spec_int ("max-slice-size",
          "Maximum slice size", "Maximum encoded slice size in bytes",
          -1, G_MAXUINT16, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxEncoderH264:la-depth
   *
   * Depth of look ahead in number frames.
   */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_H264_PROP_LA_DEPTH,
      g_param_spec_uint ("la-depth",
          "Lookahead depth", "Depth of lookahead in frames", 0, 100, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxEncoderH264:cabac
   *
   * Enable CABAC entropy coding mode for improved compression ratio,
   * at the expense that the minimum target profile is Main. Default
   * is CABAC entropy coding mode.
   */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_H264_PROP_CABAC,
      g_param_spec_boolean ("cabac",
          "Enable CABAC",
          "Enable CABAC entropy coding mode",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstMfxEncoderH264:trellis
   *
   * Enable trellis quantization
   */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_H264_PROP_TRELLIS,
      g_param_spec_enum ("trellis",
          "Trellis quantization",
          "Enable trellis quantization",
          gst_mfx_encoder_trellis_get_type (), GST_MFX_ENCODER_TRELLIS_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  /**
   * GstMfxEncoderH264:lookahead-ds
   *
   * Enable trellis Look ahead downsampling
   */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_H264_PROP_LOOKAHEAD_DS,
      g_param_spec_enum ("lookahead-ds",
          "Look ahead downsampling",
          "Look ahead downsampling",
          gst_mfx_encoder_lookahead_ds_get_type (),
          GST_MFX_ENCODER_LOOKAHEAD_DS_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  return props;
}

GST_MFX_ENCODER_DEFINE_CLASS_DATA (H264);

static void
gst_mfx_encoder_h264_init (GstMfxEncoderH264 * encoder)
{
  GST_MFX_ENCODER_GET_PRIVATE (encoder)->profile.codec = MFX_CODEC_AVC;
}

static void
gst_mfx_encoder_h264_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_mfx_encoder_h264_parent_class)->finalize (object);
}

static void
gst_mfx_encoder_h264_class_init (GstMfxEncoderH264Class * klass)
{
  GstMfxEncoderClass *const encoder_class = GST_MFX_ENCODER_CLASS (klass);
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_mfx_encoder_h264_finalize;

  encoder_class->class_data = &g_class_data;
  encoder_class->reconfigure = gst_mfx_encoder_h264_reconfigure;
  encoder_class->get_default_properties =
      gst_mfx_encoder_h264_get_default_properties;

  encoder_class->set_property = gst_mfx_encoder_h264_set_property;
  encoder_class->get_codec_data = gst_mfx_encoder_h264_get_codec_data;
}
