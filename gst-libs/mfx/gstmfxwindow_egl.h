
#ifndef GST_MFX_WINDOW_EGL_H
#define GST_MFX_WINDOW_EGL_H

#include "gstmfxdisplay.h"
#include "gstmfxwindow.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW_EGL(obj) \
	((GstMfxWindowEGL *)(obj))

GstMfxWindow *
gst_mfx_window_egl_new(GstMfxDisplay * display, guint width, guint height);

G_END_DECLS

#endif /* GST_MFX_WINDOW_EGL_H */