#include "sysdeps.h"
#include "gstmfxdisplay_egl.h"
#include "gstmfxdisplay_egl_priv.h"
#include "gstmfxwindow.h"
#include "gstmfxwindow_egl.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxtexture_egl.h"

#if USE_X11
# include "gstmfxdisplay_x11.h"
#endif
#if USE_WAYLAND
# include "gstmfxdisplay_wayland.h"
#endif

GST_DEBUG_CATEGORY(gst_debug_mfxdisplay_egl);

typedef GstMfxDisplay *(*GstMfxDisplayCreateFunc) (const gchar *);

typedef struct
{
	const gchar *type_str;
	GstMfxDisplayType type;
	GstMfxDisplayCreateFunc create_display;
} DisplayMap;

/* *INDENT-OFF* */
static const DisplayMap g_display_map[] = {
#if USE_WAYLAND
	{ "wayland",
	GST_MFX_DISPLAY_TYPE_WAYLAND,
	gst_mfx_display_wayland_new },
#endif
#if USE_X11
	{ "x11",
	GST_MFX_DISPLAY_TYPE_X11,
	gst_mfx_display_x11_new },
#endif
	{ NULL, }
};

/* ------------------------------------------------------------------------- */
/* --- EGL backend implementation                                        --- */
/* ------------------------------------------------------------------------- */

typedef struct
{
	gpointer display;
	guint display_type;
	guint gles_version;
} InitParams;

static gboolean
reset_context(GstMfxDisplayEGL * display, EGLContext gl_context)
{
	EglConfig *config;
	EglContext *ctx;

	egl_object_replace(&display->egl_context, NULL);

	if (gl_context != EGL_NO_CONTEXT)
		ctx = egl_context_new_wrapped(display->egl_display, gl_context);
	else {
		config = egl_config_new(display->egl_display, display->gles_version,
			GST_VIDEO_FORMAT_RGB);
		if (!config)
			return FALSE;

		ctx = egl_context_new(display->egl_display, config, NULL);
		egl_object_unref(config);
	}
	if (!ctx)
		return FALSE;

	egl_object_replace(&display->egl_context, ctx);
	egl_object_unref(ctx);
	return TRUE;
}

static inline gboolean
ensure_context(GstMfxDisplayEGL * display)
{
	return display->egl_context || reset_context(display, EGL_NO_CONTEXT);
}

static inline gboolean
ensure_context_is_wrapped(GstMfxDisplayEGL * display, EGLContext gl_context)
{
	return (display->egl_context &&
		display->egl_context->base.handle.p == gl_context) ||
		reset_context(display, gl_context);
}

static gboolean
gst_mfx_display_egl_bind_display(GstMfxDisplayEGL * display,
	const InitParams * params)
{
	GstMfxDisplay *native_display;
	EglDisplay *egl_display;
	const DisplayMap *m;

	for (m = g_display_map; m->type_str != NULL; m++) {
        native_display = m->create_display(NULL);

		if (native_display) {
			GST_INFO("selected backend: %s", m->type_str);
			break;
		}
	}

	if (!native_display)
		goto error_unsupported_display_type;

    gst_mfx_display_use_opengl(native_display);

	gst_mfx_display_replace(&display->display, native_display);
	gst_mfx_display_unref(native_display);

	egl_display = egl_display_new(GST_MFX_DISPLAY_NATIVE(display->display));
	if (!egl_display)
		return FALSE;

	egl_object_replace(&display->egl_display, egl_display);
	egl_object_unref(egl_display);
	display->gles_version = params->gles_version;
	return TRUE;

	/* ERRORS */
error_unsupported_display_type:
	GST_ERROR("unsupported display type (%d)", params->display_type);
	return FALSE;
}

static void
gst_mfx_display_egl_close_display(GstMfxDisplayEGL * display)
{
	gst_mfx_display_replace(&display->display, NULL);
}

static void
gst_mfx_display_egl_lock(GstMfxDisplayEGL * display)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->lock)
		klass->lock(display->display);
}

static void
gst_mfx_display_egl_unlock(GstMfxDisplayEGL * display)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->unlock)
		klass->unlock(display->display);
}

static void
gst_mfx_display_egl_sync(GstMfxDisplayEGL * display)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->sync)
		klass->sync(display->display);
	else if (klass->flush)
		klass->flush(display->display);
}

static void
gst_mfx_display_egl_flush(GstMfxDisplayEGL * display)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->flush)
		klass->flush(display->display);
}

static gboolean
gst_mfx_display_egl_get_display_info(GstMfxDisplayEGL * display,
	GstMfxDisplayInfo * info)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->get_display && !klass->get_display(display->display, info))
		return FALSE;
	return TRUE;
}

static void
gst_mfx_display_egl_get_size(GstMfxDisplayEGL * display,
	guint * width_ptr, guint * height_ptr)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->get_size)
		klass->get_size(display->display, width_ptr, height_ptr);
}

static void
gst_mfx_display_egl_get_size_mm(GstMfxDisplayEGL * display,
	guint * width_ptr, guint * height_ptr)
{
	GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display->display);

	if (klass->get_size_mm)
		klass->get_size_mm(display->display, width_ptr, height_ptr);
}

static guintptr
gst_mfx_display_egl_get_visual_id(GstMfxDisplayEGL * display,
	GstMfxWindow * window)
{
	if (!ensure_context(display))
		return 0;
	return display->egl_context->config->visual_id;
}

static GstMfxWindow *
gst_mfx_display_egl_create_window(GstMfxDisplay * display,
	guint width, guint height)
{
	return gst_mfx_window_egl_new(display, width, height);
}

static GstMfxTexture *
gst_mfx_display_egl_create_texture(GstMfxDisplay * display, GstMfxID id,
	guint target, guint format, guint width, guint height)
{
	return id != GST_MFX_ID_INVALID ?
		gst_mfx_texture_egl_new_wrapped(display, id, target, format,
		width, height) :
		gst_mfx_texture_egl_new(display, target, format, width, height);
}

static void
gst_mfx_display_egl_class_init(GstMfxDisplayEGLClass * klass)
{
	GstMfxMiniObjectClass *const object_class =
		GST_MFX_MINI_OBJECT_CLASS(klass);
	GstMfxDisplayClass *const dpy_class = GST_MFX_DISPLAY_CLASS(klass);

	GST_DEBUG_CATEGORY_INIT(gst_debug_mfxdisplay_egl, "mfxdisplay_egl", 0,
		"VA/EGL backend");

	gst_mfx_display_class_init(dpy_class);

	object_class->size = sizeof (GstMfxDisplayEGL);
	dpy_class->display_type = GST_MFX_DISPLAY_TYPE_EGL;
	dpy_class->bind_display = (GstMfxDisplayBindFunc)
		gst_mfx_display_egl_bind_display;
	dpy_class->close_display = (GstMfxDisplayCloseFunc)
		gst_mfx_display_egl_close_display;
	dpy_class->lock = (GstMfxDisplayLockFunc)
		gst_mfx_display_egl_lock;
	dpy_class->unlock = (GstMfxDisplayUnlockFunc)
		gst_mfx_display_egl_unlock;
	dpy_class->sync = (GstMfxDisplaySyncFunc)
		gst_mfx_display_egl_sync;
	dpy_class->flush = (GstMfxDisplayFlushFunc)
		gst_mfx_display_egl_flush;
	dpy_class->get_display = (GstMfxDisplayGetInfoFunc)
		gst_mfx_display_egl_get_display_info;
	dpy_class->get_size = (GstMfxDisplayGetSizeFunc)
		gst_mfx_display_egl_get_size;
	dpy_class->get_size_mm = (GstMfxDisplayGetSizeMFunc)
		gst_mfx_display_egl_get_size_mm;
	dpy_class->get_visual_id = (GstMfxDisplayGetVisualIdFunc)
		gst_mfx_display_egl_get_visual_id;
	dpy_class->create_window = (GstMfxDisplayCreateWindowFunc)
		gst_mfx_display_egl_create_window;
	dpy_class->create_texture = (GstMfxDisplayCreateTextureFunc)
		gst_mfx_display_egl_create_texture;
}

static inline const GstMfxDisplayClass *
gst_mfx_display_egl_class(void)
{
	static GstMfxDisplayEGLClass g_class;
	static gsize g_class_init = FALSE;

	if (g_once_init_enter(&g_class_init)) {
		gst_mfx_display_egl_class_init(&g_class);
		g_once_init_leave(&g_class_init, TRUE);
	}
	return GST_MFX_DISPLAY_CLASS(&g_class);
}

/**
* gst_mfx_display_egl_new:
* @display: a #GstMfxDisplay, or %NULL to pick any one
* @gles_version: the OpenGL ES version API to use
*
* Creates a new #GstMfxDisplay object suitable in EGL context. If
* the native @display is %NULL, then any type of display is picked,
* i.e. one that can be successfully opened. The @gles_version will
* further ensure the OpenGL ES API to use, or zero to indicate
* "desktop" OpenGL.
*
* Return value: a newly allocated #GstMfxDisplay object
*/
GstMfxDisplay *
gst_mfx_display_egl_new(GstMfxDisplay * display, guint gles_version)
{
	InitParams params;

	if (display) {
		params.display = GST_MFX_DISPLAY_NATIVE(display);
		params.display_type = GST_MFX_DISPLAY_VADISPLAY_TYPE(display);
	}
	else {
		params.display = NULL;
		params.display_type = GST_MFX_DISPLAY_TYPE_ANY;
	}
	params.gles_version = gles_version;
	return gst_mfx_display_new(gst_mfx_display_egl_class(),
		GST_MFX_DISPLAY_INIT_FROM_NATIVE_DISPLAY, &params);
}

/**
* gst_mfx_display_egl_new_with_native_display:
* @native_display: an EGLDisplay object
* @display_type: the display type of @native_display
* @gles_version: the OpenGL ES version API to use
*
* Creates a #GstMfxDisplay based on the native display supplied in
* as @native_display. The caller still owns the display and must call
* native display close function when all #GstMfxDisplay references
* are released. Doing so too early can yield undefined behaviour.
*
* The @gles_version will further ensure the OpenGL ES API to use, or
* zero to indicate "desktop" OpenGL.
*
* Return value: a newly allocated #GstMfxDisplay object
*/
GstMfxDisplay *
gst_mfx_display_egl_new_with_native_display(gpointer native_display,
	GstMfxDisplayType display_type, guint gles_version)
{
	InitParams params;

	g_return_val_if_fail(native_display != NULL, NULL);

	params.display = native_display;
	params.display_type = display_type;
	params.gles_version = gles_version;
	return gst_mfx_display_new(gst_mfx_display_egl_class(),
		GST_MFX_DISPLAY_INIT_FROM_NATIVE_DISPLAY, &params);
}

EglContext *
gst_mfx_display_egl_get_context(GstMfxDisplayEGL * display)
{
	return ensure_context(display) ? display->egl_context : NULL;
}

EGLDisplay
gst_mfx_display_egl_get_gl_display(GstMfxDisplayEGL * display)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_EGL(display), EGL_NO_DISPLAY);

	return display->egl_display->base.handle.p;
}

EGLContext
gst_mfx_display_egl_get_gl_context(GstMfxDisplayEGL * display)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_EGL(display), EGL_NO_CONTEXT);

	return ensure_context(display) ? display->egl_context->base.handle.p :
		EGL_NO_CONTEXT;
}

gboolean
gst_mfx_display_egl_set_gl_context(GstMfxDisplayEGL * display,
	EGLContext gl_context)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_EGL(display), FALSE);

	return ensure_context_is_wrapped(display, gl_context);
}
