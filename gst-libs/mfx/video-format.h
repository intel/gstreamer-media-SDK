/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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

#ifndef GST_MFX_VIDEO_FORMAT_H
#define GST_MFX_VIDEO_FORMAT_H

#include "sysdeps.h"

G_BEGIN_DECLS

GstVideoFormat
gst_video_format_from_mfx_fourcc (mfxU32 fourcc);

mfxU32
gst_video_format_to_mfx_fourcc (GstVideoFormat format);

#ifdef WITH_LIBVA_BACKEND
GstVideoFormat
gst_video_format_from_va_fourcc (guint fourcc);

guint
gst_video_format_to_va_fourcc (GstVideoFormat format);

mfxU32
gst_mfx_video_format_from_va_fourcc (guint fourcc);

guint
gst_mfx_video_format_to_va_fourcc (mfxU32 fourcc);

guint
gst_mfx_video_format_to_va_format (mfxU32 fourcc);
#else
DXGI_FORMAT
gst_mfx_fourcc_to_dxgi_format (mfxU32 fourcc);

DXGI_FORMAT
gst_video_format_to_dxgi_format (GstVideoFormat format);
#endif

G_END_DECLS
#endif /* GST_MFX_VIDEO_FORMAT_H */
