/*
 *  Copyright (C) 2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#ifndef GST_MFX_TEXTURE_EGL_H
#define GST_MFX_TEXTURE_EGL_H

#include "gstmfxsurface.h"

G_BEGIN_DECLS

#define GST_MFX_TEXTURE_EGL(obj) ((GstMfxTextureEGL *)(obj))

#define GST_MFX_TEXTURE_EGL_ID(texture) \
  gst_mfx_texture_egl_get_id (texture)

#define GST_MFX_TEXTURE_EGL_TARGET(texture) \
  gst_mfx_texture_egl_get_target (texture)

#define GST_MFX_TEXTURE_EGL_FORMAT(texture) \
  gst_mfx_texture_egl_get_format (texture)

#define GST_MFX_TEXTURE_EGL_WIDTH(texture) \
  gst_mfx_texture_egl_get_width (texture)

#define GST_MFX_TEXTURE_EGL_HEIGHT(texture) \
  gst_mfx_texture_egl_get_height (texture)

typedef struct _GstMfxTextureEGL GstMfxTextureEGL;

GstMfxTextureEGL *
gst_mfx_texture_egl_new (GstMfxDisplay * display, guint target,
    guint format, guint width, guint height);

GstMfxTextureEGL *
gst_mfx_texture_egl_ref (GstMfxTextureEGL * texture);

void
gst_mfx_texture_egl_unref (GstMfxTextureEGL * texture);

void
gst_mfx_texture_egl_replace (GstMfxTextureEGL ** old_texture_ptr,
    GstMfxTextureEGL * new_texture);

GstMfxID
gst_mfx_texture_egl_get_id (GstMfxTextureEGL * texture);

guint
gst_mfx_texture_egl_get_target (GstMfxTextureEGL * texture);

guint
gst_mfx_texture_egl_get_format (GstMfxTextureEGL * texture);

guint
gst_mfx_texture_egl_get_width (GstMfxTextureEGL * texture);

guint
gst_mfx_texture_egl_get_height (GstMfxTextureEGL * texture);

gboolean
gst_mfx_texture_egl_put_surface (GstMfxTextureEGL * texture,
    GstMfxSurface * surface);

G_END_DECLS

#endif /* GST_MFX_TEXTURE_EGL_H */
