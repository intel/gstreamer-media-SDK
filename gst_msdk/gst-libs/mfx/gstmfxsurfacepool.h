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

#ifndef GST_MFX_SURFACE_POOL_H
#define GST_MFX_SURFACE_POOL_H

#include "gstmfxsurface.h"
#include "gstmfxtask.h"
#include <glib.h>

G_BEGIN_DECLS

#define GST_TYPE_MFX_SURFACE_POOL (gst_mfx_surface_pool_get_type ())
G_DECLARE_FINAL_TYPE(GstMfxSurfacePool, gst_mfx_surface_pool, GST_MFX, SURFACE_POOL, GstObject)
#define GST_MFX_SURFACE_POOL(obj) \
  ((GstMfxSurfacePool *)(obj))

GstMfxSurfacePool *
gst_mfx_surface_pool_new (GstMfxSurfacePool * pool, const GstVideoInfo * info,
    gboolean memtype_is_system);

GstMfxSurfacePool *
gst_mfx_surface_pool_new_with_task (GstMfxSurfacePool * pool, GstMfxTask * task);

GstMfxSurfacePool *
gst_mfx_surface_pool_ref (GstMfxSurfacePool * pool);

void
gst_mfx_surface_pool_unref (GstMfxSurfacePool * pool);

void
gst_mfx_surface_pool_replace (GstMfxSurfacePool ** old_pool_ptr,
    GstMfxSurfacePool * new_pool);

GstMfxSurface *
gst_mfx_surface_pool_get_surface (GstMfxSurfacePool * pool);

GstMfxSurface *
gst_mfx_surface_pool_find_surface (GstMfxSurfacePool * pool,
    mfxFrameSurface1 * surface);

G_END_DECLS

#endif /* GST_MFX_SURFACE_POOL_H */
