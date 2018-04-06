/*
 *  Copyright (C) 2011-2014 Intel Corporation
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

#ifndef GST_MFX_UTILS_H264_H
#define GST_MFX_UTILS_H264_H

#include <gst/gstvalue.h>

G_BEGIN_DECLS

/* Returns a relative score for the supplied MFX profile */
guint
gst_mfx_utils_h264_get_profile_score (mfxU16 profile);

/* Returns MFX profile from a string representation */
mfxU16
gst_mfx_utils_h264_get_profile_from_string (const gchar * str);

/* Returns a string representation for the supplied H.264 profile */
const gchar *
gst_mfx_utils_h264_get_profile_string (mfxU16 profile);

/* Check if a H.264 slice contain I picture */
gboolean
gst_mfx_utils_h264_is_slice_intra (const guint8 *slice_buf, gint size);

G_END_DECLS

#endif /* GST_MFX_UTILS_H264_H */
