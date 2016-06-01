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

#ifndef GST_MFX_TEXTURE_H
#define GST_MFX_TEXTURE_H

#include "gstmfxtypes.h"
#include "gstmfxsurfaceproxy.h"

G_BEGIN_DECLS

#define GST_MFX_TEXTURE(obj) \
	((GstMfxTexture *)(obj))

#define GST_MFX_TEXTURE_ID(texture) \
	gst_mfx_texture_get_id (GST_MFX_TEXTURE (texture))

#define GST_MFX_TEXTURE_TARGET(texture) \
	gst_mfx_texture_get_target (GST_MFX_TEXTURE (texture))

#define GST_MFX_TEXTURE_FORMAT(texture) \
	gst_mfx_texture_get_format (GST_MFX_TEXTURE (texture))

#define GST_MFX_TEXTURE_WIDTH(texture) \
	gst_mfx_texture_get_width (GST_MFX_TEXTURE (texture))

#define GST_MFX_TEXTURE_HEIGHT(texture) \
	gst_mfx_texture_get_height (GST_MFX_TEXTURE (texture))

typedef struct _GstMfxTexture GstMfxTexture;

/**
* GstVaapiTextureOrientationFlags:
* @GST_VAAPI_TEXTURE_ORIENTATION_FLAG_X_INVERTED: indicates whether
*   the right row comes first in memory.
* @GST_VAAPI_TEXTURE_ORIENTATION_FLAG_Y_INVERTED: indicates whether
*   the bottom line comes first in memory.
*
* Additional flags to indicate whether the texture data is organized
* in memory with the X or Y, or both, axis inverted. e.g. if only
* @GST_VAAPI_TEXTURE_ORIENTATION_FLAG_Y_INVERTED is set, this means
* that the bottom line comes first in memory, with pixels laid out
* from the left to the right.
*/
typedef enum {
	GST_MFX_TEXTURE_ORIENTATION_FLAG_X_INVERTED = 1 << 31,
	GST_MFX_TEXTURE_ORIENTATION_FLAG_Y_INVERTED = 1 << 30,
} GstMfxTextureOrientationFlags;

GstMfxTexture *
gst_mfx_texture_new(GstMfxDisplay * display, guint target, guint format,
	guint width, guint height);

GstMfxTexture *
gst_mfx_texture_new_wrapped(GstMfxDisplay * display, guint id,
	guint target, guint format, guint width, guint height);

GstMfxTexture *
gst_mfx_texture_ref(GstMfxTexture * texture);

void
gst_mfx_texture_unref(GstMfxTexture * texture);

void
gst_mfx_texture_replace(GstMfxTexture ** old_texture_ptr,
GstMfxTexture * new_texture);

guint
gst_mfx_texture_get_target(GstMfxTexture * texture);

guint
gst_mfx_texture_get_format(GstMfxTexture * texture);

guint
gst_mfx_texture_get_width(GstMfxTexture * texture);

guint
gst_mfx_texture_get_height(GstMfxTexture * texture);

void
gst_mfx_texture_get_size(GstMfxTexture * texture, guint * width_ptr,
guint * height_ptr);

guint
gst_mfx_texture_get_orientation_flags(GstMfxTexture * texture);

void
gst_mfx_texture_set_orientation_flags(GstMfxTexture * texture,
	guint flags);

gboolean
gst_mfx_texture_put_surface(GstMfxTexture * texture,
	GstMfxSurfaceProxy * proxy);

G_END_DECLS

#endif /* GST_MFX_TEXTURE_H */
