#ifndef GST_MFX_ENCODER_H
#define GST_MFX_ENCODER_H

#include <gst/video/gstvideoutils.h>
#include <mfxvideo.h>

#include "gstmfxtaskaggregator.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER(obj) \
	((GstMfxEncoder *)(obj))

typedef struct _GstMfxEncoder GstMfxEncoder;

/**
* GstMfxEncoderStatus:
* @GST_MFX_ENCODER_STATUS_SUCCESS: Success.
* @GST_MFX_ENCODER_STATUS_NO_SURFACE: No surface left to encode.
* @GST_MFX_ENCODER_STATUS_NO_BUFFER: No coded buffer left to hold
*   the encoded picture.
* @GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN: Unknown error.
* @GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED: No memory left.
* @GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED: The requested
*   operation failed to execute properly. e.g. invalid point in time to
*   execute the operation.
* @GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_RATE_CONTROL:
*   Unsupported rate control value.
* @GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_PROFILE: Unsupported profile.
* @GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER: Invalid parameter.
* @GST_MFX_ENCODER_STATUS_ERROR_INVALID_BUFFER: Invalid buffer.
* @GST_MFX_ENCODER_STATUS_ERROR_INVALID_SURFACE: Invalid surface.
* @GST_MFX_ENCODER_STATUS_ERROR_INVALID_HEADER: Invalid header.
*
* Set of #GstMfxEncoder status codes.
*/
typedef enum
{
	GST_MFX_ENCODER_STATUS_SUCCESS = 0,
	GST_MFX_ENCODER_STATUS_NO_SURFACE = 1,
	GST_MFX_ENCODER_STATUS_NO_BUFFER = 2,
	GST_MFX_ENCODER_STATUS_MORE_DATA = 3,

	GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN = -1,
	GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED = -2,
	GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED = -3,
	GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_RATE_CONTROL = -4,
	GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_PROFILE = -5,
	GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER = -100,
	GST_MFX_ENCODER_STATUS_ERROR_INVALID_BUFFER = -101,
	GST_MFX_ENCODER_STATUS_ERROR_INVALID_SURFACE = -102,
	GST_MFX_ENCODER_STATUS_ERROR_INVALID_HEADER = -103,
} GstMfxEncoderStatus;

typedef enum {
	GST_MFX_ENCODER_LOOKAHEAD_DS_AUTO = MFX_LOOKAHEAD_DS_UNKNOWN,
	GST_MFX_ENCODER_LOOKAHEAD_DS_OFF = MFX_LOOKAHEAD_DS_OFF,
	GST_MFX_ENCODER_LOOKAHEAD_DS_2X = MFX_LOOKAHEAD_DS_2x,
	GST_MFX_ENCODER_LOOKAHEAD_DS_4X = MFX_LOOKAHEAD_DS_4x,
} GstMfxEncoderLookAheadDS;

typedef enum {
	GST_MFX_ENCODER_TRELLIS_OFF = MFX_TRELLIS_OFF,
	GST_MFX_ENCODER_TRELLIS_I = MFX_TRELLIS_I,
	GST_MFX_ENCODER_TRELLIS_IP = MFX_TRELLIS_I|MFX_TRELLIS_P,
	GST_MFX_ENCODER_TRELLIS_IPB = MFX_TRELLIS_I|MFX_TRELLIS_P|MFX_TRELLIS_B,
} GstMfxEncoderTrellis;

typedef enum {
	GST_MFX_ENCODER_PRESET_VERY_SLOW = MFX_TARGETUSAGE_BEST_QUALITY,
	GST_MFX_ENCODER_PRESET_SLOWER = MFX_TARGETUSAGE_2,
	GST_MFX_ENCODER_PRESET_SLOW = MFX_TARGETUSAGE_3,
	GST_MFX_ENCODER_PRESET_MEDIUM = MFX_TARGETUSAGE_BALANCED,
	GST_MFX_ENCODER_PRESET_FAST = MFX_TARGETUSAGE_5,
	GST_MFX_ENCODER_PRESET_FASTER = MFX_TARGETUSAGE_6,
	GST_MFX_ENCODER_PRESET_VERY_FAST = MFX_TARGETUSAGE_BEST_SPEED,
} GstMfxEncoderPreset;

typedef enum {
	GST_MFX_ENCODER_PROP_RATECONTROL = 1,
	GST_MFX_ENCODER_PROP_BITRATE,
	GST_MFX_ENCODER_PROP_FRAMERATE,
	GST_MFX_ENCODER_PROP_PRESET,
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
	GST_MFX_ENCODER_PROP_ASYNC_DEPTH,
} GstMfxEncoderProp;

/**
* GstMfxEncoderPropInfo:
* @prop: the #GstMfxEncoderProp
* @pspec: the #GParamSpec describing the associated configurable value
*
* A #GstMfxEncoderProp descriptor.
*/
typedef struct {
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
gst_mfx_encoder_ref(GstMfxEncoder * encoder);

void
gst_mfx_encoder_unref(GstMfxEncoder * encoder);

void
gst_mfx_encoder_replace(GstMfxEncoder ** old_encoder_ptr,
	GstMfxEncoder * new_encoder);

GstMfxEncoderStatus
gst_mfx_encoder_get_codec_data (GstMfxEncoder * encoder,
    GstBuffer ** out_codec_data_ptr);

gboolean
gst_mfx_encoder_set_bitrate(GstMfxEncoder * encoder, mfxU16 bitrate);

gboolean
gst_mfx_encoder_set_idr_interval(GstMfxEncoder * encoder, mfxU16 idr_interval);

gboolean
gst_mfx_encoder_set_gop_size(GstMfxEncoder * encoder, mfxU16 gop_size);

gboolean
gst_mfx_encoder_set_gop_refdist(GstMfxEncoder * encoder, gint gop_refdist);

gboolean
gst_mfx_encoder_set_num_references(GstMfxEncoder * encoder, mfxU16 num_refs);

gboolean
gst_mfx_encoder_set_num_slices(GstMfxEncoder * encoder, mfxU16 num_slices);

gboolean
gst_mfx_encoder_set_quantizer(GstMfxEncoder * encoder, guint quantizer);

gboolean
gst_mfx_encoder_set_qpi_offset(GstMfxEncoder * encoder, mfxU16 offset);

gboolean
gst_mfx_encoder_set_qpp_offset(GstMfxEncoder * encoder, mfxU16 offset);

gboolean
gst_mfx_encoder_set_qpb_offset(GstMfxEncoder * encoder, mfxU16 offset);

gboolean
gst_mfx_encoder_set_async_depth(GstMfxEncoder * encoder, mfxU16 async_depth);

GstMfxEncoderStatus
gst_mfx_encoder_start(GstMfxEncoder * encoder);

GstMfxEncoderStatus
gst_mfx_encoder_encode(GstMfxEncoder * encoder, GstVideoCodecFrame * frame);

G_END_DECLS

#endif /* GST_MFX_ENCODER_H */
