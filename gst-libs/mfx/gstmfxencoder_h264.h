#ifndef GST_MFX_ENCODER_H264_H
#define GST_MFX_ENCODER_H264_H

#include "gstmfxencoder.h"
#include "gstmfxutils_h264.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER_H264(encoder) \
	((GstMfxEncoderH264 *) (encoder))

typedef struct _GstMfxEncoderH264 GstMfxEncoderH264;

/**
* GstMfxEncoderH264Prop:
* @GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE: Number of B-frames between I
*   and P (uint).
* @GST_MFX_ENCODER_H264_PROP_LA_DEPTH: Number of slices per frame (uint).
* @GST_MFX_ENCODER_H264_PROP_CABAC: Enable CABAC entropy coding mode (bool).
* @GST_MFX_ENCODER_H264_PROP_CPB_LENGTH: Length of the CPB buffer
*   in milliseconds (uint).
*
* The set of H.264 encoder specific configurable properties.
*/
typedef enum {
	GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE = -1,
	GST_MFX_ENCODER_H264_PROP_LA_DEPTH = -2,
	GST_MFX_ENCODER_H264_PROP_CABAC = -3,
	GST_MFX_ENCODER_H264_PROP_CPB_LENGTH = -4,
} GstMfxEncoderH264Prop;

GstMfxEncoder *
gst_mfx_encoder_h264_new(GstMfxTaskAggregator * aggregator,
	GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_h264_get_default_properties(void);

gboolean
gst_mfx_encoder_h264_set_max_profile(GstMfxEncoderH264 * encoder,
	GstMfxProfile profile);

gboolean
gst_mfx_encoder_h264_get_profile_and_level(GstMfxEncoderH264 * encoder,
	GstMfxProfile * out_profile_ptr, GstMfxLevelH264 * out_level_ptr);

G_END_DECLS

#endif /*GST_MFX_ENCODER_H264_H */