#ifndef GST_MFX_ENCODER_H264_H
#define GST_MFX_ENCODER_H264_H

#include "gstmfxencoder.h"
#include "gstmfxutils_h264.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER_H264 (encoder) \
	((GstMfxEncoderH264 *) (encoder))

typedef struct _GstMfxEncoderH264 GstMfxEncoderH264;

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
	GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_h264_get_default_properties (void);

gboolean
gst_mfx_encoder_h264_set_max_profile (GstMfxEncoder * encoder, mfxU16 profile);

G_END_DECLS

#endif /*GST_MFX_ENCODER_H264_H */
