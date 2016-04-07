#ifndef GST_MFX_DECODER_H
#define GST_MFX_DECODER_H

#include "gstmfxsurfaceproxy.h"
#include "gstmfxtaskaggregator.h"

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
gst_mfx_decoder_new(GstMfxTaskAggregator * aggregator,
	mfxU32 codec, mfxU16 async_depth, gboolean mapped);

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
	GstVideoCodecFrame * frame, GstVideoInfo *info);

G_END_DECLS

#endif /* GST_MFX_DECODER_H */
