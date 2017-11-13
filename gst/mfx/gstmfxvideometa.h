/*
 *  Copyright (C) 2011-2013 Intel Corporation
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

#ifndef GST_MFX_VIDEO_META_H
#define GST_MFX_VIDEO_META_H

#include <gst/video/video.h>

#include <gst-libs/mfx/gstmfxdisplay.h>
#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst-libs/mfx/gstmfxsurfacepool.h>
#include <gst-libs/mfx/gstmfxutils_vaapi.h>

G_BEGIN_DECLS

typedef struct _GstMfxVideoMeta GstMfxVideoMeta;

#define GST_MFX_VIDEO_META_API_TYPE \
  gst_mfx_video_meta_api_get_type ()

GType
gst_mfx_video_meta_api_get_type (void);

GstMfxVideoMeta *
gst_mfx_video_meta_copy (GstMfxVideoMeta * meta);

GstMfxVideoMeta *
gst_mfx_video_meta_new (void);

GstMfxVideoMeta *
gst_mfx_video_meta_ref (GstMfxVideoMeta * meta);

void
gst_mfx_video_meta_unref (GstMfxVideoMeta * meta);

void
gst_mfx_video_meta_replace (GstMfxVideoMeta ** old_meta_ptr,
    GstMfxVideoMeta * new_meta);

GstMfxSurface *
gst_mfx_video_meta_get_surface (GstMfxVideoMeta * meta);

void
gst_mfx_video_meta_set_surface (GstMfxVideoMeta * meta,
  GstMfxSurface * surface);

gboolean
gst_mfx_video_meta_get_linear (GstMfxVideoMeta *meta);

void
gst_mfx_video_meta_set_linear (GstMfxVideoMeta *meta,
  gboolean is_linear);

GstMfxVideoMeta *
gst_buffer_get_mfx_video_meta (GstBuffer * buffer);

void
gst_buffer_set_mfx_video_meta (GstBuffer * buffer, GstMfxVideoMeta * meta);

G_END_DECLS

#endif  /* GST_MFX_VIDEO_META_H */
