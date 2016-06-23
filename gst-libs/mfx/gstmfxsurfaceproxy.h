/*
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

#ifndef GST_MFX_SURFACE_PROXY_H
#define GST_MFX_SURFACE_PROXY_H

#include <gst/video/video.h>
#include "gstmfxtask.h"
#include "gstmfxutils_vaapi.h"
#include "video-format.h"

G_BEGIN_DECLS

#define GST_MFX_SURFACE_PROXY(obj) \
	((GstMfxSurfaceProxy *) (obj))

#define GST_MFX_SURFACE_PROXY_SURFACE(proxy) \
	gst_mfx_surface_proxy_get_frame_surface (proxy)

#define GST_MFX_SURFACE_PROXY_MEMID(proxy) \
	gst_mfx_surface_proxy_get_id (proxy)

#define GST_MFX_SURFACE_PROXY_FORMAT(proxy) \
	gst_mfx_surface_proxy_get_format (proxy)

#define GST_MFX_SURFACE_PROXY_WIDTH(proxy) \
	gst_mfx_surface_proxy_get_width (proxy)

#define GST_MFX_SURFACE_PROXY_HEIGHT(proxy) \
	gst_mfx_surface_proxy_get_height (proxy)

typedef struct _GstMfxSurfacePool GstMfxSurfacePool;
typedef struct _GstMfxSurfaceProxy GstMfxSurfaceProxy;

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new (GstMfxDisplay * display, GstVideoInfo * info,
    gboolean mapped);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_from_task (GstMfxTask * task);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_from_pool (GstMfxSurfacePool * pool);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_copy (GstMfxSurfaceProxy * proxy);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_ref (GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_unref (GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_replace (GstMfxSurfaceProxy ** old_proxy_ptr,
	GstMfxSurfaceProxy * new_proxy);

mfxFrameSurface1 *
gst_mfx_surface_proxy_get_frame_surface (GstMfxSurfaceProxy * proxy);

GstMfxID
gst_mfx_surface_proxy_get_id (GstMfxSurfaceProxy * proxy);

GstMfxDisplay *
gst_mfx_surface_proxy_get_display (GstMfxSurfaceProxy * proxy);

gboolean
gst_mfx_surface_proxy_is_mapped (GstMfxSurfaceProxy * proxy);

GstVideoFormat
gst_mfx_surface_proxy_get_format (GstMfxSurfaceProxy * proxy);

guint
gst_mfx_surface_proxy_get_width (GstMfxSurfaceProxy * proxy);

guint
gst_mfx_surface_proxy_get_height (GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_get_size (GstMfxSurfaceProxy * proxy, guint * width_ptr,
	guint * height_ptr);

guint8 *
gst_mfx_surface_proxy_get_plane (GstMfxSurfaceProxy * proxy, guint plane);

guint8 *
gst_mfx_surface_proxy_get_data (GstMfxSurfaceProxy * proxy);

guint16
gst_mfx_surface_proxy_get_pitch (GstMfxSurfaceProxy * proxy, guint plane);

const GstMfxRectangle *
gst_mfx_surface_proxy_get_crop_rect (GstMfxSurfaceProxy * proxy);

VaapiImage *
gst_mfx_surface_proxy_derive_image (GstMfxSurfaceProxy * proxy);

G_END_DECLS

#endif /* GST_MFX_SURFACE_PROXY_H */
