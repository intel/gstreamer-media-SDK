/*
 *  Copyright (C) 2011-2013 Intel Corporation
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

#ifndef GST_MFX_DECODER_H
#define GST_MFX_DECODER_H

#include "gstmfxsurfaceproxy.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxprofile.h"

G_BEGIN_DECLS

#define GST_MFX_DECODER(obj) \
  ((GstMfxDecoder *)(obj))

typedef struct _GstMfxDecoder GstMfxDecoder;

/**
* GstMfxDecoderStatus:
* @GST_MFX_DECODER_STATUS_SUCCESS: Success.
* @GST_MFX_DECODER_STATUS_END_OF_STREAM: End-Of-Stream.
* @GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED: No memory left.
* @GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED: Decoder initialization failure.
* @GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CODEC: Unsupported codec.
* @GST_MFX_DECODER_STATUS_ERROR_NO_DATA: Not enough input data to decode.
* @GST_MFX_DECODER_STATUS_ERROR_NO_SURFACE: No surface left to hold the decoded picture.
* @GST_MFX_DECODER_STATUS_ERROR_INVALID_SURFACE: Invalid surface.
* @GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER: Invalid or unsupported bitstream data.
* @GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_PROFILE: Unsupported codec profile.
* @GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CHROMA_FORMAT: Unsupported chroma format.
* @GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER: Unsupported parameter.
* @GST_MFX_DECODER_STATUS_ERROR_UNKNOWN: Unknown error.
*
* Decoder status for gst_mfx_decoder_get_surface().
*/
typedef enum {
  GST_MFX_DECODER_STATUS_SUCCESS = 0,
  GST_MFX_DECODER_STATUS_END_OF_STREAM,
  GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED,
  GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED,
  GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CODEC,
  GST_MFX_DECODER_STATUS_ERROR_NO_DATA,
  GST_MFX_DECODER_STATUS_ERROR_NO_SURFACE,
  GST_MFX_DECODER_STATUS_ERROR_INVALID_SURFACE,
  GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER,
  GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_PROFILE,
  GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER,
  GST_MFX_DECODER_STATUS_ERROR_UNKNOWN = -1
} GstMfxDecoderStatus;

GstMfxDecoder *
gst_mfx_decoder_new (GstMfxTaskAggregator * aggregator,
    GstMfxProfile profile, GstVideoInfo * info,
    mfxU16 async_depth, gboolean mapped);

GstMfxDecoder *
gst_mfx_decoder_ref (GstMfxDecoder * decoder);

void
gst_mfx_decoder_unref (GstMfxDecoder * decoder);

void
gst_mfx_decoder_replace (GstMfxDecoder ** old_decoder_ptr,
    GstMfxDecoder * new_decoder);

GstMfxProfile
gst_mfx_decoder_get_profile (GstMfxDecoder * decoder);

GstMfxDecoderStatus
gst_mfx_decoder_start (GstMfxDecoder * decoder);

GstMfxDecoderStatus
gst_mfx_decoder_decode (GstMfxDecoder * decoder,
    GstVideoCodecFrame * frame, GstVideoCodecFrame ** out_frame);

G_END_DECLS

#endif /* GST_MFX_DECODER_H */
