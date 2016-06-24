#ifndef GST_MFX_ENCODER_JPEG_H
#define GST_MFX_ENCODER_JPEG_H

#include "gstmfxencoder.h"

G_BEGIN_DECLS

#define GST_MFX_ENCODER_JPEG(encoder) \
  ((GstMfxEncoderJpeg *) (encoder))

typedef struct _GstMfxEncoderJpeg GstMfxEncoderJpeg;

typedef enum {
  GST_MFX_ENCODER_JPEG_PROP_QUALITY = -1
} GstMfxEncoderJpegProp;

GstMfxEncoder *
gst_mfx_encoder_jpeg_new(GstMfxTaskAggregator * aggregator,
  	GstVideoInfo * info, gboolean mapped);

GPtrArray *
gst_mfx_encoder_jpeg_get_default_properties(void);

G_END_DECLS

#endif /* GST_MFX_ENCODER_JPEG_H */
