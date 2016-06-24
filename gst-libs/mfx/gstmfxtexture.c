/*
 *  Copyright (C) 2012-2013 Intel Corporation
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
#include "gstmfxtexture.h"
#include "gstmfxtexture_priv.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_texture_ref
#undef gst_mfx_texture_unref
#undef gst_mfx_texture_replace

#define GST_MFX_TEXTURE_ORIENTATION_FLAGS \
  (GST_MFX_TEXTURE_ORIENTATION_FLAG_X_INVERTED | \
  GST_MFX_TEXTURE_ORIENTATION_FLAG_Y_INVERTED)

static void
gst_mfx_texture_init (GstMfxTexture * texture, GstMfxID id,
    guint target, guint format, guint width, guint height)
{
  texture->is_wrapped = id != GST_MFX_ID_INVALID;
  GST_MFX_OBJECT_ID (texture) = texture->is_wrapped ? id : 0;
  texture->gl_target = target;
  texture->gl_format = format;
  texture->width = width;
  texture->height = height;
}

static inline gboolean
gst_mfx_texture_allocate (GstMfxTexture * texture)
{
  return GST_MFX_TEXTURE_GET_CLASS (texture)->allocate (texture);
}

GstMfxTexture *
gst_mfx_texture_new_internal (const GstMfxTextureClass * klass,
    GstMfxDisplay * display, GstMfxID id, guint target, guint format,
    guint width, guint height)
{
  GstMfxTexture *texture;

  g_return_val_if_fail (target != 0, NULL);
  g_return_val_if_fail (format != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  texture = gst_mfx_object_new (GST_MFX_MINI_OBJECT_CLASS (klass), display);
  if (!texture)
    return NULL;

  gst_mfx_texture_init (texture, id, target, format, width, height);
  if (!gst_mfx_texture_allocate (texture))
    goto error;
  return texture;

error:
  gst_mfx_object_unref (texture);
  return NULL;
}

/**
 * gst_mfx_texture_new:
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
 * The application shall maintain the live GL context itself.
 *
 * Return value: the newly created #GstMfxTexture object
 */
GstMfxTexture *
gst_mfx_texture_new (GstMfxDisplay * display, guint target, guint format,
    guint width, guint height)
{
  GstMfxDisplayClass *dpy_class;

  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (gst_mfx_display_has_opengl (display), NULL);

  dpy_class = GST_MFX_DISPLAY_GET_CLASS (display);
  if (G_UNLIKELY (!dpy_class->create_texture))
    return NULL;
  return dpy_class->create_texture (display, GST_MFX_ID_INVALID, target,
      format, width, height);
}

/**
 * gst_mfx_texture_new_wrapped:
 * @display: a #GstMfxDisplay
 * @texture_id: the foreign GL texture name to use
 * @target: the target to which the texture is bound
 * @format: the format of the pixel data
 * @width: the suggested width, in pixels
 * @height: the suggested height, in pixels
 *
 * Creates a texture with the specified dimensions, @target and
 * @format. Note that only GL_TEXTURE_2D @target and GL_RGBA or
 * GL_BGRA formats are supported at this time.
 *
 * The size arguments @width and @height are only a suggestion. Should
 * this be 0x0, then the actual size of the allocated texture storage
 * would be either inherited from the original texture storage, if any
 * and/or if possible, or derived from the VA surface in subsequent
 * gst_mfx_texture_put_surface () calls.
 *
 * The application shall maintain the live GL context itself.
 *
 * Return value: the newly created #GstMfxTexture object
 */
GstMfxTexture *
gst_mfx_texture_new_wrapped (GstMfxDisplay * display, guint id,
    guint target, guint format, guint width, guint height)
{
  GstMfxDisplayClass *dpy_class;

  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (gst_mfx_display_has_opengl (display), NULL);

  dpy_class = GST_MFX_DISPLAY_GET_CLASS (display);
  if (G_UNLIKELY (!dpy_class->create_texture))
    return NULL;
  return dpy_class->create_texture (display, id, target, format, width, height);
}

/**
 * gst_mfx_texture_ref:
 * @texture: a #GstMfxTexture
 *
 * Atomically increases the reference count of the given @texture by one.
 *
 * Returns: The same @texture argument
 */
GstMfxTexture *
gst_mfx_texture_ref (GstMfxTexture * texture)
{
  return gst_mfx_texture_ref_internal (texture);
}

/**
 * gst_mfx_texture_unref:
 * @texture: a #GstMfxTexture
 *
 * Atomically decreases the reference count of the @texture by one. If
 * the reference count reaches zero, the texture will be free'd.
 */
void
gst_mfx_texture_unref (GstMfxTexture * texture)
{
  gst_mfx_texture_unref_internal (texture);
}

/**
 * gst_mfx_texture_replace:
 * @old_texture_ptr: a pointer to a #GstMfxTexture
 * @new_texture: a #GstMfxTexture
 *
 * Atomically replaces the texture held in @old_texture_ptr
 * with @new_texture. This means that @old_texture_ptr shall reference
 * a valid texture. However, @new_texture can be NULL.
 */
void
gst_mfx_texture_replace (GstMfxTexture ** old_texture_ptr,
    GstMfxTexture * new_texture)
{
  gst_mfx_texture_replace_internal (old_texture_ptr, new_texture);
}

/**
 * gst_mfx_texture_get_target:
 * @texture: a #GstMfxTexture
 *
 * Returns the @texture target type
 *
 * Return value: the texture target
 */
guint
gst_mfx_texture_get_target (GstMfxTexture * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return GST_MFX_TEXTURE_TARGET (texture);
}

/**
 * gst_mfx_texture_get_format
 * @texture: a #GstMfxTexture
 *
 * Returns the @texture format
 *
 * Return value: the texture format
 */
guint
gst_mfx_texture_get_format (GstMfxTexture * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return GST_MFX_TEXTURE_FORMAT (texture);
}

/**
 * gst_mfx_texture_get_width:
 * @texture: a #GstMfxTexture
 *
 * Returns the @texture width.
 *
 * Return value: the texture width, in pixels
 */
guint
gst_mfx_texture_get_width (GstMfxTexture * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return GST_MFX_TEXTURE_WIDTH (texture);
}

/**
 * gst_mfx_texture_get_height:
 * @texture: a #GstMfxTexture
 *
 * Returns the @texture height.
 *
 * Return value: the texture height, in pixels.
 */
guint
gst_mfx_texture_get_height (GstMfxTexture * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return GST_MFX_TEXTURE_HEIGHT (texture);
}

/**
 * gst_mfx_texture_get_size:
 * @texture: a #GstMfxTexture
 * @width_ptr: return location for the width, or %NULL
 * @height_ptr: return location for the height, or %NULL
 *
 * Retrieves the dimensions of a #GstMfxTexture.
 */
void
gst_mfx_texture_get_size (GstMfxTexture * texture,
    guint * width_ptr, guint * height_ptr)
{
  g_return_if_fail (texture != NULL);

  if (width_ptr)
    *width_ptr = GST_MFX_TEXTURE_WIDTH (texture);

  if (height_ptr)
    *height_ptr = GST_MFX_TEXTURE_HEIGHT (texture);
}

/**
 * gst_mfx_texture_get_orientation_flags:
 * @texture: a #GstMfxTexture
 *
 * Retrieves the texture memory layout flags, i.e. orientation.
 *
 * Return value: the #GstMfxTextureOrientationFlags.
 */
guint
gst_mfx_texture_get_orientation_flags (GstMfxTexture * texture)
{
  g_return_val_if_fail (texture != NULL, 0);

  return GST_MFX_TEXTURE_FLAGS (texture) & GST_MFX_TEXTURE_ORIENTATION_FLAGS;
}

/**
 * gst_mfx_texture_set_orientation_flags:
 * @texture: a #GstMfxTexture
 * @flags: a bitmask of #GstMfxTextureOrientationFlags
 *
 * Reset the texture orientation flags to the supplied set of
 * @flags. This completely replaces the previously installed
 * flags. So, should they still be needed, then they shall be
 * retrieved first with gst_mfx_texture_get_orientation_flags ().
 */
void
gst_mfx_texture_set_orientation_flags (GstMfxTexture * texture, guint flags)
{
  g_return_if_fail (texture != NULL);
  g_return_if_fail ((flags & ~GST_MFX_TEXTURE_ORIENTATION_FLAGS) == 0);

  GST_MFX_TEXTURE_FLAG_UNSET (texture, GST_MFX_TEXTURE_ORIENTATION_FLAGS);
  GST_MFX_TEXTURE_FLAG_SET (texture, flags);
}

/**
 * gst_mfx_texture_put_surface:
 * @texture: a #GstMfxTexture
 * @surface: a #GstMfxSurfaceProxy
 * @flags: postprocessing flags. See #GstMfxTextureRenderFlags
 *
 * Renders the @surface into the àtexture. The @flags specify how
 * de-interlacing (if needed), color space conversion, scaling and
 * other postprocessing transformations are performed.
 *
 * Return value: %TRUE on success
 */
gboolean
gst_mfx_texture_put_surface (GstMfxTexture * texture,
    GstMfxSurfaceProxy * proxy)
{
  const GstMfxTextureClass *klass;

  g_return_val_if_fail (texture != NULL, FALSE);
  g_return_val_if_fail (proxy != NULL, FALSE);

  klass = GST_MFX_TEXTURE_GET_CLASS (texture);
  if (!klass)
    return FALSE;

  return klass->put_surface (texture, proxy);
}
