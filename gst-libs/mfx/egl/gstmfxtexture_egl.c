/*
 *  Copyright (C) 2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "sysdeps.h"

#include <libdrm/drm_fourcc.h>
#include "gstmfxtexture_egl.h"
#include "gstmfxutils_egl.h"
#include "gstmfxsurface_vaapi.h"
#include "gstmfxdisplay_egl.h"
#include "gstmfxdisplay_egl_priv.h"
#include "gstmfxprimebufferproxy.h"
#include "gstmfxutils_vaapi.h"

#define DEBUG 1

/**
 * GstMfxTextureEGL:
 *
 * Base object for EGL texture wrapper.
 */
struct _GstMfxTextureEGL
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GstMfxID texture_id;
  GstMfxDisplay *display;

  EglContext *egl_context;
  EGLImageKHR egl_image;
  guint gl_target;
  guint gl_format;
  guint width;
  guint height;
};

typedef struct
{
  GstMfxTextureEGL *texture;
  GstMfxSurface *surface;
  const GstMfxRectangle *crop_rect;
  gboolean success;             /* result */
} UploadSurfaceArgs;

static gboolean
do_bind_texture_unlocked (GstMfxTextureEGL * texture,
    GstMfxSurface * surface)
{
  EglContext *const ctx = texture->egl_context;
  EglVTable *const vtable = egl_context_get_vtable (ctx, FALSE);
  GstMfxPrimeBufferProxy *buffer_proxy;
  VaapiImage *image;

  if (gst_mfx_surface_has_video_memory (surface)) {
    GLint attribs[23], *attrib;

    buffer_proxy = gst_mfx_prime_buffer_proxy_new_from_surface (surface);
    if (!buffer_proxy)
      return FALSE;

    image = gst_mfx_prime_buffer_proxy_get_vaapi_image (buffer_proxy);

    texture->width = vaapi_image_get_width (image);
    texture->height = vaapi_image_get_height (image);

    attrib = attribs;
    *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
    *attrib++ = DRM_FORMAT_ARGB8888;
    *attrib++ = EGL_WIDTH;
    *attrib++ = texture->width;
    *attrib++ = EGL_HEIGHT;
    *attrib++ = texture->height;
    *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
    *attrib++ = GST_MFX_PRIME_BUFFER_PROXY_HANDLE (buffer_proxy);
    *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    *attrib++ = vaapi_image_get_offset (image, 0);
    *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    *attrib++ = vaapi_image_get_pitch (image, 0);
    *attrib++ = EGL_NONE;

    texture->egl_image =
        vtable->eglCreateImageKHR (ctx->display->base.handle.p, EGL_NO_CONTEXT,
          EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer) NULL, attribs);
    if (!texture->egl_image) {
      GST_ERROR ("failed to import VA buffer (RGBA) into EGL image");
      goto error;
    }

    texture->texture_id =
        egl_create_texture_from_egl_image (texture->egl_context,
          texture->gl_target, texture->egl_image);
    if (!texture->texture_id) {
      GST_ERROR ("failed to create texture from EGL image");
      goto error;
    }

    vaapi_image_unref (image);
    gst_mfx_prime_buffer_proxy_unref (buffer_proxy);
  } else {
    texture->texture_id =
        egl_create_texture_from_data (texture->egl_context, GL_TEXTURE_2D,
          GL_BGRA_EXT, texture->width, texture->height,
          gst_mfx_surface_get_plane (surface, 0));
    if (!texture->texture_id) {
      GST_ERROR ("failed to create texture from raw data");
      return FALSE;
    }
  }

  return TRUE;
error:
  {
    vaapi_image_unref (image);
    gst_mfx_prime_buffer_proxy_unref (buffer_proxy);
    return FALSE;
  }
}

static void
do_bind_texture (UploadSurfaceArgs * args)
{
  GstMfxTextureEGL *const texture = args->texture;
  EglContextState old_cs;

  args->success = FALSE;

  GST_MFX_DISPLAY_LOCK (texture->display);
  if (egl_context_set_current (texture->egl_context, TRUE, &old_cs)) {
    args->success = do_bind_texture_unlocked (texture, args->surface);
    egl_context_set_current (texture->egl_context, FALSE, &old_cs);
  }
  GST_MFX_DISPLAY_UNLOCK (texture->display);
}

static void
destroy_objects (GstMfxTextureEGL * texture)
{
  EglContext *const ctx = texture->egl_context;
  EglVTable *const vtable = egl_context_get_vtable (ctx, FALSE);

  if (texture->egl_image != EGL_NO_IMAGE_KHR) {
    vtable->eglDestroyImageKHR (ctx->display->base.handle.p,
        texture->egl_image);
    texture->egl_image = EGL_NO_IMAGE_KHR;
  }
}

static void
do_destroy_texture_unlocked (GstMfxTextureEGL * texture)
{
  destroy_objects (texture);

  if (texture->texture_id) {
    egl_destroy_texture (texture->egl_context, texture->texture_id);
    texture->texture_id = 0;
  }
}

static void
do_destroy_texture (GstMfxTextureEGL * texture)
{
  EglContextState old_cs;

  GST_MFX_DISPLAY_LOCK (texture->display);
  if (egl_context_set_current (texture->egl_context, TRUE, &old_cs)) {
    do_destroy_texture_unlocked (texture);
    egl_context_set_current (texture->egl_context, FALSE, &old_cs);
  }
  GST_MFX_DISPLAY_UNLOCK (texture->display);
  egl_object_replace (&texture->egl_context, NULL);
}

static void
gst_mfx_texture_egl_destroy (GstMfxTextureEGL * texture)
{
  egl_context_run (texture->egl_context,
      (EglContextRunFunc) do_destroy_texture, texture);
}

static void
gst_mfx_texture_egl_init (GstMfxTextureEGL * texture, GstMfxDisplay * display,
    guint target, guint format, guint width, guint height)
{
  texture->display = gst_mfx_display_ref (display);
  texture->gl_target = target;
  texture->gl_format = format;
  texture->width = width;
  texture->height = height;

  egl_object_replace (&texture->egl_context,
      GST_MFX_DISPLAY_EGL_CONTEXT (texture->display));
}

static void
gst_mfx_texture_egl_finalize (GstMfxTextureEGL * texture)
{
  gst_mfx_texture_egl_destroy (texture);

  gst_mfx_display_unref (texture->display);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_texture_egl_class (void)
{
  static const GstMfxMiniObjectClass GstMfxTextureEGLClass = {
    sizeof (GstMfxTextureEGL),
    (GDestroyNotify) gst_mfx_texture_egl_finalize
  };
  return &GstMfxTextureEGLClass;
}

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

GstMfxTextureEGL *
gst_mfx_texture_egl_new (GstMfxDisplay * display, guint target,
    guint format, guint width, guint height)
{
  GstMfxTextureEGL *texture;

  g_return_val_if_fail (GST_MFX_IS_DISPLAY_EGL (display), NULL);
  g_return_val_if_fail (target != 0, NULL);
  g_return_val_if_fail (format != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  texture = (GstMfxTextureEGL *)
      gst_mfx_mini_object_new0 (gst_mfx_texture_egl_class ());
  if (!texture)
    return NULL;

  gst_mfx_texture_egl_init (texture, display, target, format, width, height);
  return texture;
}

GstMfxTextureEGL *
gst_mfx_texture_egl_ref (GstMfxTextureEGL * texture)
{
  g_return_val_if_fail (texture != NULL, NULL);

  return (GstMfxTextureEGL *)
      gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (texture));
}

void
gst_mfx_texture_egl_unref (GstMfxTextureEGL * texture)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (texture));
}

void
gst_mfx_texture_egl_replace (GstMfxTextureEGL ** old_texture_ptr,
    GstMfxTextureEGL * new_texture)
{
  g_return_if_fail (old_texture_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_texture_ptr,
      GST_MFX_MINI_OBJECT (new_texture));
}

GstMfxID
gst_mfx_texture_egl_get_id (GstMfxTextureEGL * texture)
{
  g_return_val_if_fail (texture != NULL, GST_MFX_ID_INVALID);

  return texture->texture_id;
}

guint
gst_mfx_texture_egl_get_target (GstMfxTextureEGL * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return texture->gl_target;
}

guint
gst_mfx_texture_egl_get_format (GstMfxTextureEGL * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return texture->gl_format;
}

guint
gst_mfx_texture_egl_get_width (GstMfxTextureEGL * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return texture->width;
}

guint
gst_mfx_texture_egl_get_height (GstMfxTextureEGL * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return texture->height;
}

gboolean
gst_mfx_texture_egl_put_surface (GstMfxTextureEGL * texture,
    GstMfxSurface * surface)
{
  UploadSurfaceArgs args = { texture, surface };

  return egl_context_run (texture->egl_context,
      (EglContextRunFunc) do_bind_texture, &args) && args.success;
}
