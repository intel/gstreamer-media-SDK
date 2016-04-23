#include "sysdeps.h"
#include <drm/drm_fourcc.h>
#include "gstmfxtexture.h"
#include "gstmfxtexture_egl.h"
#include "gstmfxtexture_priv.h"
#include "gstmfxutils_egl.h"
#include "gstmfxdisplay_egl.h"
#include "gstmfxdisplay_egl_priv.h"
#include "gstmfxprimebufferproxy.h"
#include "gstmfxutils_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

typedef struct
{
	GstMfxTextureEGL *texture;
	GstMfxSurfaceProxy *proxy;
	const GstMfxRectangle *crop_rect;
	gboolean success;             /* result */
} UploadSurfaceArgs;

static gboolean
do_bind_texture_unlocked(GstMfxTextureEGL * texture, GstMfxSurfaceProxy * proxy)
{
	EglContext *const ctx = texture->egl_context;
	EglVTable *const vtable = egl_context_get_vtable(ctx, FALSE);
	GstMfxTexture *const base_texture = GST_MFX_TEXTURE(texture);

    if (GST_MFX_SURFACE_PROXY_MEMID(proxy) != GST_MFX_ID_INVALID) {
        GLint attribs[23], *attrib;
        GstMfxPrimeBufferProxy *buffer_proxy;
        VaapiImage *image;

        buffer_proxy = gst_mfx_prime_buffer_proxy_new_from_surface(proxy);
        if (!buffer_proxy)
            return FALSE;

        image = gst_mfx_surface_proxy_derive_image(proxy);
        if (!image)
            return FALSE;

        GST_MFX_TEXTURE_WIDTH(base_texture) = vaapi_image_get_width(image);
        GST_MFX_TEXTURE_HEIGHT(base_texture) = vaapi_image_get_height(image);

        attrib = attribs;
        *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
        *attrib++ = DRM_FORMAT_ARGB8888;
        *attrib++ = EGL_WIDTH;
        *attrib++ = vaapi_image_get_width(image);
        *attrib++ = EGL_HEIGHT;
        *attrib++ = vaapi_image_get_height(image);
        *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
        *attrib++ = GST_MFX_PRIME_BUFFER_PROXY_HANDLE(buffer_proxy);
        *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
        *attrib++ = vaapi_image_get_offset(image, 0);
        *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
        *attrib++ = vaapi_image_get_pitch(image, 0);
        *attrib++ = EGL_NONE;

        texture->egl_image = vtable->eglCreateImageKHR(
            ctx->display->base.handle.p, EGL_NO_CONTEXT,
            EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attribs);
        if (!texture->egl_image) {
            GST_ERROR("failed to import VA buffer (RGBA) into EGL image\n");
            return FALSE;
        }

        GST_MFX_TEXTURE_ID(texture) = egl_create_texture_from_egl_image(
            texture->egl_context, base_texture->gl_target, texture->egl_image);
        if (!GST_MFX_OBJECT_ID(base_texture)) {
            return FALSE;
        }

        gst_mfx_prime_buffer_proxy_unref(buffer_proxy);
        vaapi_image_unref(image);
    }
    else {
        GST_MFX_TEXTURE_ID(texture) = egl_create_texture_from_data(
            texture->egl_context, GL_TEXTURE_2D, GL_BGRA_EXT,
            base_texture->width, base_texture->height,
            gst_mfx_surface_proxy_get_plane(proxy, 0));
        if (!GST_MFX_OBJECT_ID(base_texture)) {
            return FALSE;
        }
    }

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
		args->success = do_bind_texture_unlocked(texture, args->proxy);
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
}

static void
do_destroy_texture_unlocked(GstMfxTextureEGL * texture)
{
	//GstMfxTexture *const base_texture = GST_MFX_TEXTURE(texture);
	const GLuint texture_id = GST_MFX_TEXTURE_ID(texture);

	destroy_objects(texture);

	if (texture_id) {
		//if (!base_texture->is_wrapped)
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
	GstMfxSurfaceProxy * proxy)
{
	UploadSurfaceArgs args = { texture, proxy };

	return egl_context_run(texture->egl_context,
		(EglContextRunFunc)do_bind_texture, &args) && args.success;
}

static gboolean
gst_mfx_texture_egl_create_surface(GstMfxTextureEGL * texture)
{
    egl_object_replace(&texture->egl_context,
        GST_MFX_DISPLAY_EGL_CONTEXT(GST_MFX_OBJECT_DISPLAY(texture)));
    return TRUE;
}

static void
gst_mfx_texture_egl_class_init(GstMfxTextureEGLClass * klass)
{
	GstMfxObjectClass *const object_class = GST_MFX_OBJECT_CLASS(klass);
	GstMfxTextureClass *const texture_class = GST_MFX_TEXTURE_CLASS(klass);

	object_class->finalize = (GstMfxObjectFinalizeFunc)
		gst_mfx_texture_egl_destroy;
    texture_class->allocate = (GstMfxTextureAllocateFunc)
		gst_mfx_texture_egl_create_surface;
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
