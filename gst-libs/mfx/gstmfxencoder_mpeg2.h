#ifndef GST_MFX_ENCODER_MPEG2_H
#define GST_MFX_ENCODER_MPEG2_H

#include "gstmfxencoder.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER_MPEG2(encoder) \
	((GstMfxEncoderMpeg2 *) (encoder))

typedef struct _GstMfxEncoderMpeg2 GstMfxEncoderMpeg2;

GstMfxEncoder *
gst_mfx_encoder_mpeg2_new (GstMfxTaskAggregator * aggregator,
	GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_mpeg2_get_default_properties(void);

G_END_DECLS

#endif /* GST_MFX_ENCODER_MPEG2_H */