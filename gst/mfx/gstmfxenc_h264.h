#ifndef GST_MFXENC_H264_H
#define GST_MFXENC_H264_H

#include <gst/gst.h>
#include "gstmfxenc.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXENC_H264 \
  (gst_mfxenc_h264_get_type ())
#define GST_MFXENC_H264_CAST(obj) \
  ((GstMfxEncH264 *)(obj))
#define GST_MFXENC_H264(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC_H264, \
  GstMfxEncH264))
#define GST_MFXENC_H264_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC_H264, \
  GstMfxEncH264Class))
#define GST_MFXENC_H264_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC_H264, \
  GstMfxEncH264Class))
#define GST_IS_MFXENC_H264(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC_H264))
#define GST_IS_MFXENC_H264_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC_H264))

typedef struct _GstMfxEncH264 GstMfxEncH264;
typedef struct _GstMfxEncH264Class GstMfxEncH264Class;

struct _GstMfxEncH264
{
  /*< private >*/
  GstMfxEnc parent_instance;

  guint is_avc : 1; /* [FALSE]=byte-stream (default); [TRUE]=avcC */
};

struct _GstMfxEncH264Class
{
  /*< private >*/
  GstMfxEncClass parent_class;
};

GType
gst_mfxenc_h264_get_type (void);

G_END_DECLS

#endif /* GST_MFXENC_H264_H */
