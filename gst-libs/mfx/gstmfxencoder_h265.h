#ifndef GST_MFX_ENCODER_H265_H
#define GST_MFX_ENCODER_H265_H

#include "gstmfxencoder.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER_H265 (encoder) \
	((GstMfxEncoderH265 *) (encoder))

typedef struct _GstMfxEncoderH265 GstMfxEncoderH265;

/**
* GstMfxEncoderH265Prop:
* @GST_MFX_ENCODER_H265_PROP_LA_DEPTH:
* @GST_MFX_ENCODER_H265_PROP_LOOKAHEAD_DS:
*
* The set of H.265 encoder specific configurable properties.
*/
typedef enum {
	GST_MFX_ENCODER_H265_PROP_LA_DEPTH = -1,
	GST_MFX_ENCODER_H265_PROP_LOOKAHEAD_DS = -2,
} GstMfxEncoderH265Prop;

GstMfxEncoder *
gst_mfx_encoder_h265_new (GstMfxTaskAggregator * aggregator,
	GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_h265_get_default_properties (void);

G_END_DECLS

#endif /*GST_MFX_ENCODER_H265_H */
