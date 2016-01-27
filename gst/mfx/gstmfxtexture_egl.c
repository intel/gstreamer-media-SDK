#include "sysdeps.h"
#include "gstmfxtexture.h"
#include "gstmfxtexture_egl.h"
#include "gstmfxtexture_priv.h"
#include "gstmfxutils_egl.h"
#include "gstmfxdisplay_egl.h"
#include "gstmfxdisplay_egl_priv.h"
#include "gstvaapibufferproxy_priv.h"

#include <drm/drm_fourcc.h>

#define GST_MFX_TEXTURE_EGL(texture) \
	((GstMfxTextureEGL *) (texture))

typedef struct _GstMfxTextureEGL GstMfxTextureEGL;
typedef struct _GstMfxTextureEGLClass GstMfxTextureEGLClass;

/**
* GstMfxTextureEGL:
*
* Base object for EGL texture wrapper.
*/
struct _GstMfxTextureEGL
{
	/*< private >*/
	GstMfxTexture parent_instance;

	EglContext *egl_context;
	EGLImageKHR egl_image;
	GstMfxSurface *surface;
};

/**
* GstMfxTextureEGLClass:
*
* Base class for EGL texture wrapper.
*/
struct _GstMfxTextureEGLClass
{
	/*< private >*/
	GstMfxTextureClass parent_class;
};

typedef struct
{
	GstMfxTextureEGL *texture;
	gboolean success;             /* result */
} CreateTextureArgs;

typedef struct
{
	GstMfxTextureEGL *texture;
	GstMfxSurface *surface;
	const GstMfxRectangle *crop_rect;
	gboolean success;             /* result */
} UploadSurfaceArgs;

static gboolean
do_bind_texture_unlocked(GstMfxTextureEGL * texture, GstMfxSurface * surface)
{
	EglContext *const ctx = texture->egl_context;
	EglVTable *const vtable = egl_context_get_vtable(ctx, FALSE);
	GstMfxTexture *const base_texture = GST_MFX_TEXTURE(texture);
	GLuint texture_id;

	GLint attribs[23], *attrib;
	GstVaapiBufferProxy *buffer_proxy;
	GstVaapiImage *image;
	VAImage *va_image;
	uint32_t i;

	buffer_proxy = gst_vaapi_buffer_proxy_new_from_object(GST_MFX_OBJECT(texture->surface));
	if (!buffer_proxy)
		return FALSE;

	image = gst_mfx_surface_derive_image(texture->surface);
	if (!image)
		return FALSE;
	gst_vaapi_image_get_image(image, va_image);

	attrib = attribs;
	*attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
	*attrib++ = DRM_FORMAT_NV12;
	*attrib++ = EGL_WIDTH;
	*attrib++ = GST_VAAPI_IMAGE_WIDTH(image);
	*attrib++ = EGL_HEIGHT;
	*attrib++ = GST_VAAPI_IMAGE_HEIGHT(image);
	for (i = 0; i < gst_vaapi_image_get_plane_count(image); i++) {
		*attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT + 3 * i;
		*attrib++ = GST_VAAPI_BUFFER_PROXY_HANDLE(buffer_proxy);
		*attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT + 3 * i;
		*attrib++ = va_image->offsets[i];
		*attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT + 3 * i;
		*attrib++ = gst_vaapi_image_get_pitch(image, i);
	}
	*attrib++ = EGL_NONE;
	texture->egl_image = vtable->eglCreateImageKHR(ctx->display, EGL_NO_CONTEXT,
		EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attribs);
	if (!texture->egl_image)
		return FALSE;

	texture_id = egl_create_texture_with_egl_image(texture->egl_context,
		base_texture->gl_target, texture->egl_image);
	if (!texture_id)
		return FALSE;
	GST_MFX_TEXTURE_ID(texture) = texture_id;

	return TRUE;
}

static void
do_bind_texture(UploadSurfaceArgs * args)
{
	GstMfxTextureEGL *const texture = args->texture;
	EglContextState old_cs;

	args->success = FALSE;

	GST_MFX_OBJECT_LOCK_DISPLAY(texture);
	if (egl_context_set_current(texture->egl_context, TRUE, &old_cs)) {
		args->success = do_bind_texture_unlocked(texture, args->surface);
		egl_context_set_current(texture->egl_context, FALSE, &old_cs);
	}
	GST_MFX_OBJECT_UNLOCK_DISPLAY(texture);
}

static void
destroy_objects(GstMfxTextureEGL * texture)
{
	EglContext *const ctx = texture->egl_context;
	EglVTable *const vtable = egl_context_get_vtable(ctx, FALSE);

	if (texture->egl_image != EGL_NO_IMAGE_KHR) {
		vtable->eglDestroyImageKHR(ctx->display->base.handle.p,
			texture->egl_image);
		texture->egl_image = EGL_NO_IMAGE_KHR;
	}
	gst_mfx_object_replace(&texture->surface, NULL);
}

static void
do_destroy_texture_unlocked(GstMfxTextureEGL * texture)
{
	GstMfxTexture *const base_texture = GST_MFX_TEXTURE(texture);
	const GLuint texture_id = GST_MFX_TEXTURE_ID(texture);

	destroy_objects(texture);

	if (texture_id) {
		if (!base_texture->is_wrapped)
			egl_destroy_texture(texture->egl_context, texture_id);
		GST_MFX_TEXTURE_ID(texture) = 0;
	}
}

static void
do_destroy_texture(GstMfxTextureEGL * texture)
{
	EglContextState old_cs;

	GST_MFX_OBJECT_LOCK_DISPLAY(texture);
	if (egl_context_set_current(texture->egl_context, TRUE, &old_cs)) {
		do_destroy_texture_unlocked(texture);
		egl_context_set_current(texture->egl_context, FALSE, &old_cs);
	}
	GST_MFX_OBJECT_UNLOCK_DISPLAY(texture);
	egl_object_replace(&texture->egl_context, NULL);
}

static void
gst_mfx_texture_egl_destroy(GstMfxTextureEGL * texture)
{
	egl_context_run(texture->egl_context,
		(EglContextRunFunc)do_destroy_texture, texture);
}

static gboolean
gst_mfx_texture_egl_put_surface(GstMfxTextureEGL * texture,
	GstMfxSurface * surface, const GstMfxRectangle * crop_rect)
{
	UploadSurfaceArgs args = { texture, surface, crop_rect };

	return egl_context_run(texture->egl_context,
		(EglContextRunFunc)do_bind_texture, &args) && args.success;
}

static void
gst_mfx_texture_egl_class_init(GstMfxTextureEGLClass * klass)
{
	GstMfxObjectClass *const object_class = GST_MFX_OBJECT_CLASS(klass);
	GstMfxTextureClass *const texture_class = GST_MFX_TEXTURE_CLASS(klass);

	object_class->finalize = (GstMfxObjectFinalizeFunc)
		gst_mfx_texture_egl_destroy;
	texture_class->put_surface = (GstMfxTexturePutSurfaceFunc)
		gst_mfx_texture_egl_put_surface;
}

#define gst_mfx_texture_egl_finalize gst_mfx_texture_egl_destroy
GST_MFX_OBJECT_DEFINE_CLASS_WITH_CODE(GstMfxTextureEGL,
	gst_mfx_texture_egl, gst_mfx_texture_egl_class_init(&g_class));

/**
* gst_mfx_texture_egl_new:
* @display: a #GstMfxDisplay
* @target: the target to which the texture is bound
* @format: the format of the pixel data
* @width: the requested width, in pixels
* @height: the requested height, in pixels
*
* Creates a texture with the specified dimensions, @target and
* @format. Note that only GL_TEXTURE_2D @target and GL_RGBA or
* GL_BGRA formats are supported at this time.
*
* The application shall maintain the live EGL context itself. That
* is, gst_mfx_window_egl_make_current() must be called beforehand,
* or any other function like eglMakeCurrent() if the context is
* managed outside of this library.
*
* Return value: the newly created #GstMfxTexture object
*/
GstMfxTexture *
gst_mfx_texture_egl_new(GstMfxDisplay * display, guint target,
	guint format, guint width, guint height)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_EGL(display), NULL);

	return gst_mfx_texture_new_internal(GST_MFX_TEXTURE_CLASS
		(gst_mfx_texture_egl_class()), display, GST_MFX_ID_INVALID, target,
		format, width, height);
}

/**
* gst_mfx_texture_egl_new_wrapped:
* @display: a #GstMfxDisplay
* @texture_id: the foreign GL texture name to use
* @target: the target to which the texture is bound
* @format: the format of the pixel data
* @width: the texture width, in pixels
* @height: the texture height, in pixels
*
* Creates a texture from an existing GL texture, with the specified
* @target and @format. Note that only GL_TEXTURE_2D @target and
* GL_RGBA or GL_BGRA formats are supported at this time.
*
* The application shall maintain the live EGL context itself. That
* is, gst_mfx_window_egl_make_current() must be called beforehand,
* or any other function like eglMakeCurrent() if the context is
* managed outside of this library.
*
* Return value: the newly created #GstMfxTexture object
*/
GstMfxTexture *
gst_mfx_texture_egl_new_wrapped(GstMfxDisplay * display,
	guint texture_id, guint target, GLenum format, guint width, guint height)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_EGL(display), NULL);
	g_return_val_if_fail(texture_id != GL_NONE, NULL);

	return gst_mfx_texture_new_internal(GST_MFX_TEXTURE_CLASS
		(gst_mfx_texture_egl_class()), display, texture_id, target, format,
		width, height);
}
