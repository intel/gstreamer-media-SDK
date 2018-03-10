/*
 *  gstmfxsubpicturecomposition.h - MFX subpicture composition abstraction
 *
 *  Copyright (C) 2017 Intel Corporation
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

#ifndef GST_MFX_SURFACE_COMPOSITION_H
#define GST_MFX_SURFACE_COMPOSITION_H

#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst/video/video-overlay-composition.h>

G_BEGIN_DECLS

#define GST_TYPE_MFX_SURFACE_COMPOSITION (gst_mfx_surface_composition_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxSurfaceComposition, gst_mfx_surface_composition,
    GST_MFX, SURFACE_COMPOSITION, GstObject)
#define GST_MFX_SURFACE_COMPOSITION(obj) ((GstMfxSurfaceComposition *)(obj))
     
typedef struct _GstMfxSubpicture GstMfxSubpicture;
struct _GstMfxSubpicture
{
  GstMfxSurface *surface;
  gfloat global_alpha;
  GstMfxRectangle sub_rect;
};

GstMfxSurfaceComposition *
gst_mfx_surface_composition_new (GstMfxSurface *
    base_surface, GstVideoOverlayComposition * overlay);

GstMfxSurfaceComposition *
gst_mfx_surface_composition_ref (GstMfxSurfaceComposition * composition);

void
gst_mfx_surface_composition_unref (GstMfxSurfaceComposition * composition);

void
gst_mfx_surface_composition_replace (GstMfxSurfaceComposition ** old_composition_ptr,
    GstMfxSurfaceComposition * new_composition);

GstMfxSurface *
gst_mfx_surface_composition_get_base_surface (GstMfxSurfaceComposition * composition);

guint
gst_mfx_surface_composition_get_num_subpictures (GstMfxSurfaceComposition * composition);

GstMfxSubpicture *
gst_mfx_surface_composition_get_subpicture (GstMfxSurfaceComposition * composition,
    guint index);

G_END_DECLS
#endif /* GST_MFX_SURFACE_COMPOSITION_H */
