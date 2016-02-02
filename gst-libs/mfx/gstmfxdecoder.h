/*
*  gstmfxdecoder.h - VA decoder abstraction
*
*  Copyright (C) 2010-2011 Splitted-Desktop Systems
*    Author: Gwenole Beauchesne <gwenole.beauchesne@splitted-desktop.com>
*  Copyright (C) 2011-2013 Intel Corporation
*    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#include <gst/gstbuffer.h>
#include <gst/video/gstvideoutils.h>

#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurface_priv.h"
#include "gstmfxobjectpool.h"
#include "gstmfxcontext.h"

G_BEGIN_DECLS

#define GST_MFX_DECODER(obj) \
	((GstMfxDecoder *)(obj))

typedef struct _GstMfxDecoder GstMfxDecoder;

struct _GstMfxDecoder
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxDisplay *display;
	GstMfxContext *context;
	GstMfxObjectPool *pool;
	GAsyncQueue *surfaces;
	GList *work_surfaces;

	mfxSession session;
	mfxFrameAllocRequest req;
	mfxVideoParam param;
	mfxBitstream bs;
	mfxU32 codec;

	GstMfxContextAllocatorVaapi *alloc_ctx;

	gboolean decoder_inited;
};

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
	GST_MFX_DECODER_STATUS_READY,
	GST_MFX_DECODER_STATUS_END_OF_STREAM,
	GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED,
	GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED,
	GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CODEC,
	GST_MFX_DECODER_STATUS_ERROR_NO_DATA,
	GST_MFX_DECODER_STATUS_ERROR_NO_SURFACE,
	GST_MFX_DECODER_STATUS_ERROR_INVALID_SURFACE,
	GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER,
	GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_PROFILE,
	GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CHROMA_FORMAT,
	GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER,
	GST_MFX_DECODER_STATUS_ERROR_UNKNOWN = -1
} GstMfxDecoderStatus;

GstMfxDecoder *
gst_mfx_decoder_ref(GstMfxDecoder * decoder);

void
gst_mfx_decoder_unref(GstMfxDecoder * decoder);

void
gst_mfx_decoder_replace(GstMfxDecoder ** old_decoder_ptr,
	GstMfxDecoder * new_decoder);

mfxU32
gst_mfx_decoder_get_codec(GstMfxDecoder * decoder);

GstMfxDecoderStatus
gst_mfx_decoder_get_surface_proxy(GstMfxDecoder * decoder,
	GstMfxSurfaceProxy ** out_proxy_ptr);

GstMfxDecoderStatus
gst_mfx_decoder_decode(GstMfxDecoder * decoder,
	GstVideoCodecFrame * frame);


GstMfxDecoder *
gst_mfx_decoder_new(GstMfxDisplay * display,
	GstMfxContextAllocatorVaapi *allocator, mfxU32 codec_id);

G_END_DECLS

#endif /* GST_MFX_DECODER_H */
