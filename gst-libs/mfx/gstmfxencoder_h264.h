/*
 *  Copyright (C) 2011-2014 Intel Corporation
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

#ifndef GST_MFX_ENCODER_H264_H
#define GST_MFX_ENCODER_H264_H

#include "gstmfxencoder.h"
#include "gstmfxencoder_priv.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_ENCODER_H264 (gst_mfx_encoder_h264_get_type ())
G_DECLARE_FINAL_TYPE(GstMfxEncoderH264, gst_mfx_encoder_h264, GST_MFX, ENCODER_H264, GstMfxEncoder)

#define GST_MFX_ENCODER_H264_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFX_ENCODER_H264, \
  GstMfxEncoderH264Class))

#define GST_MFX_ENCODER_H264 (encoder) \
  ((GstMfxEncoderH264 *) (encoder))

/**
 * GstMfxEncoderH264Prop:
 * @GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE:
 * @GST_MFX_ENCODER_H264_PROP_LA_DEPTH:
 * @GST_MFX_ENCODER_H264_PROP_CABAC: Enable CABAC entropy coding mode (bool).
 * @GST_MFX_ENCODER_H264_PROP_TRELLIS:
 * @GST_MFX_ENCODER_H264_PROP_LOOKAHEAD_DS:
 *
 * The set of H.264 encoder specific configurable properties.
 */
typedef enum {
  GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE = -1,
  GST_MFX_ENCODER_H264_PROP_LA_DEPTH = -2,
  GST_MFX_ENCODER_H264_PROP_CABAC = -3,
  GST_MFX_ENCODER_H264_PROP_TRELLIS = -5,
  GST_MFX_ENCODER_H264_PROP_LOOKAHEAD_DS = -6,
} GstMfxEncoderH264Prop;

GstMfxEncoder *
gst_mfx_encoder_h264_new (GstMfxTaskAggregator * aggregator,
  	const GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_h264_get_default_properties (void);

G_END_DECLS

#endif /*GST_MFX_ENCODER_H264_H */
