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

#ifndef GST_MFX_ENCODER_H
#define GST_MFX_ENCODER_H

#include <gst/video/gstvideoutils.h>
#include "gstmfxtaskaggregator.h"
#include "gstmfxprofile.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_ENCODER (gst_mfx_encoder_get_type ())
G_DECLARE_DERIVABLE_TYPE (GstMfxEncoder, gst_mfx_encoder, GST_MFX,
    ENCODER, GstObject)

typedef struct _GstMfxEncoder GstMfxEncoder;

typedef enum
{
  GST_MFX_ENCODER_STATUS_SUCCESS = 0,
  GST_MFX_ENCODER_STATUS_NO_SURFACE = 1,
  GST_MFX_ENCODER_STATUS_NO_BUFFER = 2,
  GST_MFX_ENCODER_STATUS_MORE_DATA = 3,

  GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN = -1,
  GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED = -2,
  GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED = -3,
  GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER = -4,
} GstMfxEncoderStatus;

typedef enum
{
  GST_MFX_ENCODER_LOOKAHEAD_DS_AUTO = MFX_LOOKAHEAD_DS_UNKNOWN,
  GST_MFX_ENCODER_LOOKAHEAD_DS_OFF = MFX_LOOKAHEAD_DS_OFF,
  GST_MFX_ENCODER_LOOKAHEAD_DS_2X = MFX_LOOKAHEAD_DS_2x,
  GST_MFX_ENCODER_LOOKAHEAD_DS_4X = MFX_LOOKAHEAD_DS_4x,
} GstMfxEncoderLookAheadDS;

typedef enum
{
  GST_MFX_ENCODER_TRELLIS_OFF = MFX_TRELLIS_OFF,
  GST_MFX_ENCODER_TRELLIS_I = MFX_TRELLIS_I,
  GST_MFX_ENCODER_TRELLIS_IP = MFX_TRELLIS_I | MFX_TRELLIS_P,
  GST_MFX_ENCODER_TRELLIS_IPB = MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B,
} GstMfxEncoderTrellis;

typedef enum
{
  GST_MFX_ENCODER_PRESET_VERY_SLOW = MFX_TARGETUSAGE_BEST_QUALITY,
  GST_MFX_ENCODER_PRESET_SLOWER = MFX_TARGETUSAGE_2,
  GST_MFX_ENCODER_PRESET_SLOW = MFX_TARGETUSAGE_3,
  GST_MFX_ENCODER_PRESET_MEDIUM = MFX_TARGETUSAGE_BALANCED,
  GST_MFX_ENCODER_PRESET_FAST = MFX_TARGETUSAGE_5,
  GST_MFX_ENCODER_PRESET_FASTER = MFX_TARGETUSAGE_6,
  GST_MFX_ENCODER_PRESET_VERY_FAST = MFX_TARGETUSAGE_BEST_SPEED,
} GstMfxEncoderPreset;

typedef enum
{
  GST_MFX_ENCODER_PROP_RATECONTROL = 1,
  GST_MFX_ENCODER_PROP_BITRATE,
  GST_MFX_ENCODER_PROP_PRESET,
  GST_MFX_ENCODER_PROP_BRC_MULTIPLIER,
  GST_MFX_ENCODER_PROP_MAX_BUFFER_SIZE,
  GST_MFX_ENCODER_PROP_VBV_MAX_BITRATE,
  GST_MFX_ENCODER_PROP_IDR_INTERVAL,
  GST_MFX_ENCODER_PROP_GOP_SIZE,
  GST_MFX_ENCODER_PROP_GOP_REFDIST,
  GST_MFX_ENCODER_PROP_NUM_REFS,
  GST_MFX_ENCODER_PROP_NUM_SLICES,
  GST_MFX_ENCODER_PROP_QUANTIZER,
  GST_MFX_ENCODER_PROP_QPI,
  GST_MFX_ENCODER_PROP_QPP,
  GST_MFX_ENCODER_PROP_QPB,
  GST_MFX_ENCODER_PROP_MBBRC,
  GST_MFX_ENCODER_PROP_EXTBRC,
  GST_MFX_ENCODER_PROP_ADAPTIVE_I,
  GST_MFX_ENCODER_PROP_ADAPTIVE_B,
  GST_MFX_ENCODER_PROP_B_PYRAMID,
  GST_MFX_ENCODER_PROP_ACCURACY,
  GST_MFX_ENCODER_PROP_CONVERGENCE,
  GST_MFX_ENCODER_PROP_ASYNC_DEPTH,
} GstMfxEncoderProp;

/**
 * GstMfxEncoderPropInfo:
 * @prop: the #GstMfxEncoderProp
 * @pspec: the #GParamSpec describing the associated configurable value
 *
 * A #GstMfxEncoderProp descriptor.
 */
typedef struct
{
  const gint prop;
  GParamSpec *const pspec;
} GstMfxEncoderPropInfo;

GType
gst_mfx_encoder_preset_get_type (void);

GType
gst_mfx_encoder_trellis_get_type (void);

GType
gst_mfx_encoder_lookahead_ds_get_type (void);

GstMfxEncoder *
gst_mfx_encoder_ref (GstMfxEncoder * encoder);

void
gst_mfx_encoder_unref (GstMfxEncoder * encoder);

void
gst_mfx_encoder_replace (GstMfxEncoder ** old_encoder_ptr,
    GstMfxEncoder * new_encoder);

GstMfxEncoderStatus
gst_mfx_encoder_get_codec_data (GstMfxEncoder * encoder,
    GstBuffer ** out_codec_data_ptr);

GstMfxEncoderStatus
gst_mfx_encoder_set_codec_state (GstMfxEncoder * encoder,
    GstVideoCodecState * state);

GstMfxEncoderStatus
gst_mfx_encoder_set_property (GstMfxEncoder * encoder, gint prop_id,
    const GValue * value);

void
gst_mfx_encoder_set_peer_id (GstMfxEncoder * encoder, gint peer_id);

gboolean
gst_mfx_encoder_set_async_depth (GstMfxEncoder * encoder, mfxU16 async_depth);

void
gst_mfx_encoder_set_profile (GstMfxEncoder * encoder, mfxU16 profile);

GstMfxProfile
gst_mfx_encoder_get_profile (GstMfxEncoder * encoder);

gboolean
gst_mfx_encoder_set_gop_refdist (GstMfxEncoder * encoder, gint gop_refdist);

gboolean
gst_mfx_encoder_set_num_references (GstMfxEncoder * encoder, mfxU16 num_refs);

gboolean
gst_mfx_encoder_set_quantizer (GstMfxEncoder * encoder, guint quantizer);

gboolean
gst_mfx_encoder_set_qpi_offset (GstMfxEncoder * encoder, mfxU16 offset);

gboolean
gst_mfx_encoder_set_qpp_offset (GstMfxEncoder * encoder, mfxU16 offset);

gboolean
gst_mfx_encoder_set_qpb_offset (GstMfxEncoder * encoder, mfxU16 offset);

GstMfxEncoderStatus gst_mfx_encoder_prepare (GstMfxEncoder * encoder);

GstMfxEncoderStatus
gst_mfx_encoder_encode (GstMfxEncoder * encoder, GstVideoCodecFrame * frame);

GstMfxEncoderStatus
gst_mfx_encoder_flush (GstMfxEncoder * encoder, GstVideoCodecFrame ** frame);

GType
gst_mfx_encoder_get_type (void);

G_END_DECLS
#endif /* GST_MFX_ENCODER_H */
