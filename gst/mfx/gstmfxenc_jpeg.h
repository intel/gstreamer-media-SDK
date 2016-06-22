#ifndef GST_MFXENC_JPEG_H
#define GST_MFXENC_JPEG_H

#include <gst/gst.h>
#include "gstmfxenc.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXENC_JPEG \
	(gst_mfxenc_jpeg_get_type ())
#define GST_MFXENC_JPEG_CAST(obj) \
	((GstMfxEncJpeg *)(obj))
#define GST_MFXENC_JPEG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC_JPEG, \
	GstMfxEncJpeg))
#define GST_MFXENC_JPEG_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC_JPEG, \
	GstMfxEncJpegClass))
#define GST_MFXENC_JPEG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC_JPEG, \
	GstMfxEncJpegClass))
#define GST_IS_MFXENC_JPEG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC_JPEG))
#define GST_IS_MFXENC_JPEG_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC_JPEG))

typedef struct _GstMfxEncJpeg GstMfxEncJpeg;
typedef struct _GstMfxEncJpegClass GstMfxEncJpegClass;

struct _GstMfxEncJpeg
{
	/*< private >*/
	GstMfxEnc parent_instance;
};

struct _GstMfxEncJpegClass
{
	/*< private >*/
	GstMfxEncClass parent_class;
};

GType
gst_mfxenc_jpeg_get_type(void);

G_END_DECLS

#endif /* GST_MFXENC_JPEG_H */