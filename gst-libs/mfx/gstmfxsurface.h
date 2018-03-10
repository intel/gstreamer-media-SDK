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

#ifndef GST_MFX_SURFACE_H
#define GST_MFX_SURFACE_H

#include "sysdeps.h"

#include "gstmfxcontext.h"
#include "gstmfxtask.h"
#include "gstmfxtypes.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_SURFACE (gst_mfx_surface_get_type ())
#define GST_MFX_SURFACE(obj) ((GstMfxSurface *) (obj))
#define GST_MFX_SURFACE_FRAME_SURFACE(surface) \
  gst_mfx_surface_get_frame_surface (surface)
#define GST_MFX_SURFACE_ID(surface) \
  gst_mfx_surface_get_id (surface)
#define GST_MFX_SURFACE_FORMAT(surface) \
  gst_mfx_surface_get_format (surface)
#define GST_MFX_SURFACE_WIDTH(surface) \
  gst_mfx_surface_get_width (surface)
#define GST_MFX_SURFACE_HEIGHT(surface) \
  gst_mfx_surface_get_height (surface)

typedef struct _GstMfxSurface GstMfxSurface;
typedef struct _GstMfxSurfacePool GstMfxSurfacePool;

GType
gst_mfx_surface_get_type (void);

GstMfxSurface *
gst_mfx_surface_new (const GstVideoInfo * info);

GstMfxSurface *
gst_mfx_surface_new_from_task (GstMfxTask * task);

GstMfxSurface *
gst_mfx_surface_new_from_pool (GstMfxSurfacePool * pool);

GstMfxSurface *
gst_mfx_surface_copy (GstMfxSurface * surface);

GstMfxSurface *
gst_mfx_surface_ref (GstMfxSurface * surface);

void
gst_mfx_surface_unref (GstMfxSurface * surface);

void
gst_mfx_surface_replace (GstMfxSurface ** old_surface_ptr,
    GstMfxSurface * new_surface);

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface (GstMfxSurface * surface);

GstMfxID
gst_mfx_surface_get_id (GstMfxSurface * surface);

GstVideoFormat
gst_mfx_surface_get_format (GstMfxSurface * surface);

guint
gst_mfx_surface_get_width (GstMfxSurface * surface);

guint
gst_mfx_surface_get_height (GstMfxSurface * surface);

void
gst_mfx_surface_get_size (GstMfxSurface * surface, guint * width_ptr,
    guint * height_ptr);

guint8 *
gst_mfx_surface_get_plane (GstMfxSurface * surface, guint plane);

guint16
gst_mfx_surface_get_pitch (GstMfxSurface * surface, guint plane);

guint
gst_mfx_surface_get_data_size (GstMfxSurface * surface);

GstMfxRectangle *
gst_mfx_surface_get_crop_rect (GstMfxSurface * surface);

GstMfxContext *
gst_mfx_surface_get_context (GstMfxSurface * surface);

gboolean
gst_mfx_surface_has_video_memory (GstMfxSurface * surface);

gboolean
gst_mfx_surface_map (GstMfxSurface * surface);

void
gst_mfx_surface_unmap (GstMfxSurface * surface);

G_END_DECLS
#endif /* GST_MFX_SURFACE_H */
