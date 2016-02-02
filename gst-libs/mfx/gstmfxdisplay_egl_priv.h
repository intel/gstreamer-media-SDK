#ifndef GST_MFX_DISPLAY_EGL_PRIV_H
#define GST_MFX_DISPLAY_EGL_PRIV_H

#include "gstmfxwindow.h"
#include "gstmfxdisplay_egl.h"
#include "gstmfxdisplay_priv.h"
#include "gstmfxutils_egl.h"

G_BEGIN_DECLS

#define GST_MFX_IS_DISPLAY_EGL(display) \
	((display) != NULL && \
	GST_MFX_DISPLAY_GET_CLASS_TYPE (display) == GST_MFX_DISPLAY_TYPE_EGL)

#define GST_MFX_DISPLAY_EGL_CLASS(klass) \
	((GstMfxDisplayEGLClass *)(klass))

#define GST_MFX_DISPLAY_EGL_GET_CLASS(obj) \
	GST_MFX_DISPLAY_EGL_CLASS (GST_MFX_DISPLAY_GET_CLASS (obj))

/**
* GST_MFX_DISPLAY_EGL_DISPLAY:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to #EglDisplay wrapper for @display.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_EGL_DISPLAY
#define GST_MFX_DISPLAY_EGL_DISPLAY(display) \
	(GST_MFX_DISPLAY_EGL (display)->egl_display)

/**
* GST_MFX_DISPLAY_EGL_CONTEXT:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to #EglContext wrapper for @display.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_EGL_CONTEXT
#define GST_MFX_DISPLAY_EGL_CONTEXT(display) \
	gst_mfx_display_egl_get_context (GST_MFX_DISPLAY_EGL (display))

typedef struct _GstMfxDisplayEGLClass GstMfxDisplayEGLClass;

/**
* GstMfxDisplayEGL:
*
* VA/EGL display wrapper.
*/
struct _GstMfxDisplayEGL
{
	/*< private >*/
	GstMfxDisplay parent_instance;

	GstMfxDisplay *display;
	EglDisplay *egl_display;
	EglContext *egl_context;
	guint gles_version;
};

/**
* GstMfxDisplayEGLClass:
*
* VA/EGL display wrapper clas.
*/
struct _GstMfxDisplayEGLClass
{
	/*< private >*/
	GstMfxDisplayClass parent_class;
};

EglContext *
gst_mfx_display_egl_get_context(GstMfxDisplayEGL * display);

G_END_DECLS

#endif /* GST_MFX_DISPLAY_EGL_PRIV_H */
