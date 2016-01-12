#ifndef GST_MFX_WINDOW_DRM_H
#define GST_MFX_WINDOW_DRM_H

#include "gstmfxdisplay.h"
#include "gstmfxwindow.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW_DRM(obj) \
    ((GstMfxWindowDRM *)(obj))

typedef struct _GstMfxWindowDRM GstMfxWindowDRM;

GstMfxWindow *
gst_mfx_window_drm_new (GstMfxDisplay * display, guint width, guint height);

G_END_DECLS

#endif /* GST_MFX_WINDOW_DRM_H */