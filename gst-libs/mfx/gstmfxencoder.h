#ifndef GST_MFX_ENCODER_H
#define GST_MFX_ENCODER_H

#include "gstmfxtaskaggregator.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER(obj) \
	((GstMfxEncoder *)(obj))

typedef struct _GstMfxEncoder GstMfxEncoder;

/**
* GstMfxEncoderStatus:
* @GST_MFX_ENCODER_STATUS_SUCCESS: Success.
* @GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED: No memory left.
* @GST_MFX_ENCODER_STATUS_ERROR_INIT_FAILED: Encoder initialization failure.
* @GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_CODEC: Unsupported codec.
* @GST_MFX_ENCODER_STATUS_NO_SURFACE: No surface left to encode.
* @GST_MFX_ENCODER_STATUS_NO_BUFFER: No coded buffer left to hold
*   the encoded picture.
* @GST_MFX_ENCODER_STATUS_ERROR_INVALID_SURFACE: Invalid surface.
* @GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_PROFILE: Unsupported codec profile.
* @GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER: Unsupported parameter.
* @GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN: Unknown error.
*
*/
typedef enum {
	GST_MFX_ENCODER_STATUS_SUCCESS = 0,
	GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED,
	GST_MFX_ENCODER_STATUS_ERROR_INIT_FAILED,
	GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_CODEC,
	GST_MFX_ENCODER_STATUS_ERROR_NO_BUFFER,
	GST_MFX_ENCODER_STATUS_ERROR_NO_SURFACE,
	GST_MFX_ENCODER_STATUS_ERROR_INVALID_SURFACE,
	GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_PROFILE,
	GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER,
	GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN = -1
} GstMfxEncoderStatus;

GstMfxEncoder *
gst_mfx_encoder_new(GstMfxTaskAggregator * aggregator,
	mfxU32 codec, mfxU16 async_depth, GstVideoInfo * info, gboolean mapped);

GstMfxEncoder *
gst_mfx_encoder_ref(GstMfxEncoder * encoder);

void
gst_mfx_encoder_unref(GstMfxEncoder * encoder);

void
gst_mfx_encoder_replace(GstMfxEncoder ** old_encoder_ptr,
GstMfxEncoder * new_encoder);

void
gst_mfx_encoder_set_bitrate(GstMfxEncoder * encoder, mfxU16 bitrate);

gboolean
gst_mfx_encoder_set_target_usage(GstMfxEncoder * encoder, mfxU16 target_usage);

gboolean
gst_mfx_encoder_set_rate_control(GstMfxEncoder * encoder, mfxU16 rc_method);

GstMfxEncoderStatus
gst_mfx_encoder_start(GstMfxEncoder * encoder);

GstMfxEncoderStatus
gst_mfx_encoder_encode(GstMfxEncoder * encoder, GstVideoCodecFrame * frame);

G_END_DECLS

#endif /* GST_MFX_ENCODER_H */
