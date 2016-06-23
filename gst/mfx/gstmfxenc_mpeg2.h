#ifndef GST_MFXENC_MPEG2_H
#define GST_MFXENC_MPEG2_H

#include <gst/gst.h>
#include "gstmfxenc.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXENC_MPEG2 \
  (gst_mfxenc_mpeg2_get_type ())
#define GST_MFXENC_MPEG2_CAST(obj) \
  ((GstMfxEncMpeg2 *)(obj))
#define GST_MFXENC_MPEG2(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC_MPEG2, \
  GstMfxEncMpeg2))
#define GST_MFXENC_MPEG2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC_MPEG2, \
  GstMfxEncMpeg2Class))
#define GST_MFXENC_MPEG2_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC_MPEG2, \
  GstMfxEncMpeg2Class))
#define GST_IS_MFXENC_MPEG2(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC_MPEG2))
#define GST_IS_MFXENC_MPEG2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC_MPEG2))

typedef struct _GstMfxEncMpeg2 GstMfxEncMpeg2;
typedef struct _GstMfxEncMpeg2Class GstMfxEncMpeg2Class;

struct _GstMfxEncMpeg2
{
  /*< private >*/
  GstMfxEnc parent_instance;
};

struct _GstMfxEncMpeg2Class
{
  /*< private >*/
  GstMfxEncClass parent_class;
};

GType
gst_mfxenc_mpeg2_get_type(void);

G_END_DECLS

#endif /* GST_MFXENC_MPEG2_H */
