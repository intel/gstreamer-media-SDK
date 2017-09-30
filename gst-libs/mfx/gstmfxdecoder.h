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

#include "gstmfxtaskaggregator.h"
#include "gstmfxprofile.h"

G_BEGIN_DECLS
#define GST_TYPE_MFX_DECODER (gst_mfx_decoder_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxDecoder, gst_mfx_decoder, GST_MFX, DECODER,
    GstObject)
#define GST_MFX_DECODER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFX_DECODER, GstMfxDecoder))

typedef enum
{
  GST_MFX_DECODER_STATUS_SUCCESS = 0,
  GST_MFX_DECODER_STATUS_CONFIGURED,
  GST_MFX_DECODER_STATUS_REINIT,
  GST_MFX_DECODER_STATUS_FLUSHED,
  GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED,
  GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED,
  GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CODEC,
  GST_MFX_DECODER_STATUS_ERROR_MORE_DATA,
  GST_MFX_DECODER_STATUS_ERROR_MORE_SURFACE,
  GST_MFX_DECODER_STATUS_ERROR_INVALID_SURFACE,
  GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER,
  GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_PROFILE,
  GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER,
  GST_MFX_DECODER_STATUS_ERROR_UNKNOWN = -1
} GstMfxDecoderStatus;

GstMfxDecoder *
gst_mfx_decoder_new (GstMfxTaskAggregator * aggregator, GstMfxProfile profile,
    const GstVideoInfo * info, GByteArray * codec_data, mfxU16 async_depth,
    gboolean live_mode, gboolean is_autoplugged);

GstMfxDecoder *
gst_mfx_decoder_ref (GstMfxDecoder * decoder);

void
gst_mfx_decoder_unref (GstMfxDecoder * decoder);

void
gst_mfx_decoder_replace (GstMfxDecoder ** old_decoder_ptr,
    GstMfxDecoder * new_decoder);

const mfxFrameAllocRequest *
gst_mfx_decoder_get_request (GstMfxDecoder * decoder);

gboolean
gst_mfx_decoder_get_frame (GstMfxDecoder * decoder,
    GstVideoCodecFrame ** out_frame, gboolean discarded);

void
gst_mfx_decoder_set_video_info (GstMfxDecoder * decoder, GstVideoInfo * info);

void
gst_mfx_decoder_skip_corrupted_frames (GstMfxDecoder * decoder);

void
gst_mfx_decoder_decide_output_memtype (GstMfxDecoder * decoder,
    gboolean memtype_is_video);

gboolean
gst_mfx_decoder_reset (GstMfxDecoder * decoder);

GstMfxDecoderStatus
gst_mfx_decoder_decode (GstMfxDecoder * decoder, GstVideoCodecFrame * frame);

GstMfxDecoderStatus
gst_mfx_decoder_flush (GstMfxDecoder * decoder);

G_END_DECLS
#endif /* GST_MFX_DECODER_H */
