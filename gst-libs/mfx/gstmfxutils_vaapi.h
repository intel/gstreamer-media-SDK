/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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

#ifndef GST_MFX_UTILS_VAAPI_H
#define GST_MFX_UTILS_VAAPI_H

#include "gstmfxdisplay.h"
#include <gst/video/video.h>

G_BEGIN_DECLS
#define GST_TYPE_VAAPI_IMAGE (vaapi_image_get_type ())
G_DECLARE_FINAL_TYPE (VaapiImage, vaapi_image, VAAPI, IMAGE, GstObject)
#define VAAPI_IMAGE(obj) ((VaapiImage *) (obj))

VaapiImage *
vaapi_image_new (GstMfxDisplay * display,
    guint width, guint height, GstVideoFormat format);

VaapiImage *
vaapi_image_new_with_image (GstMfxDisplay * display, const VAImage * va_image);

VaapiImage *
vaapi_image_ref (VaapiImage * image);

void
vaapi_image_unref (VaapiImage * image);

void
vaapi_image_replace (VaapiImage ** old_image_ptr, VaapiImage * new_image);

gboolean
vaapi_image_get_image (VaapiImage * image, VAImage * va_image);

GstVideoFormat
vaapi_image_get_format (VaapiImage * image);

guint
vaapi_image_get_width (VaapiImage * image);

guint
vaapi_image_get_height (VaapiImage * image);

void vaapi_image_get_size (VaapiImage * image, guint * pwidth,
    guint * pheight);

gboolean
vaapi_image_map (VaapiImage * image);

gboolean
vaapi_image_unmap (VaapiImage * image);

guint
vaapi_image_get_plane_count (VaapiImage * image);

guchar *
vaapi_image_get_plane (VaapiImage * image, guint plane);

guint
vaapi_image_get_pitch (VaapiImage * image, guint plane);

guint
vaapi_image_get_offset (VaapiImage * image, guint plane);

guint
vaapi_image_get_data_size (VaapiImage * image);

gboolean
vaapi_check_status (VAStatus status, const gchar * msg);

G_END_DECLS
#endif /* GST_MFX_UTILS_VAAPI_H */
