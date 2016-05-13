#ifndef GST_MFX_DISPLAY_EGL_H
#define GST_MFX_DISPLAY_EGL_H

#include <EGL/egl.h>
#include "gstmfxdisplay.h"

G_BEGIN_DECLS

typedef struct _GstMfxDisplayEGL GstMfxDisplayEGL;

#define GST_MFX_DISPLAY_EGL(obj) \
	((GstMfxDisplayEGL *)(obj))

GstMfxDisplay *
gst_mfx_display_egl_new(const gchar * display_name, guint gles_version);

EGLDisplay
gst_mfx_display_egl_get_gl_display(GstMfxDisplayEGL * display);

EGLContext
gst_mfx_display_egl_get_gl_context(GstMfxDisplayEGL * display);

gboolean
gst_mfx_display_egl_set_gl_context(GstMfxDisplayEGL * display,
EGLContext gl_context);

G_END_DECLS

#endif /* GST_MFX_DISPLAY_EGL_H */
