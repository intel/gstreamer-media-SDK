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

#ifndef GST_MFX_ENCODER_H265_H
#define GST_MFX_ENCODER_H265_H

#include "gstmfxencoder.h"
#include "gstmfxencoder_priv.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_ENCODER_H265 (gst_mfx_encoder_h265_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxEncoderH265, gst_mfx_encoder_h265, GST_MFX,
    ENCODER_H265, GstMfxEncoder)
#define GST_MFX_ENCODER_H265_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFX_ENCODER_H265, \
  GstMfxEncoderH265Class))
#define GST_MFX_ENCODER_H265 (encoder) \
  ((GstMfxEncoderH265 *) (encoder))

/**
 * GstMfxEncoderH265Prop:
 * @GST_MFX_ENCODER_H265_PROP_LA_DEPTH:
 * @GST_MFX_ENCODER_H265_PROP_LOOKAHEAD_DS:
 *
 * The set of H.265 encoder specific configurable properties.
 */
typedef enum
{
  GST_MFX_ENCODER_H265_PROP_LA_DEPTH = -1,
  GST_MFX_ENCODER_H265_PROP_LOOKAHEAD_DS = -2,
} GstMfxEncoderH265Prop;

GstMfxEncoder *
gst_mfx_encoder_h265_new (GstMfxTaskAggregator * aggregator,
    const GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_h265_get_default_properties (void);

G_END_DECLS
#endif /*GST_MFX_ENCODER_H265_H */
