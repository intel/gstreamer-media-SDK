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

#ifndef GST_MFX_SUBPICTURE_COMPOSITION_H
#define GST_MFX_SUBPICTURE_COMPOSITION_H

#include <gst-libs/mfx/gstmfxdisplay.h>
#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst/video/video-overlay-composition.h>

G_BEGIN_DECLS

#define GST_MFX_SUBPICTURE_COMPOSITION(obj) \
    ((GstMfxSubpictureComposition *)(obj))

typedef struct _GstMfxSubpictureComposition GstMfxSubpictureComposition;
typedef struct _GstMfxSubpicture GstMfxSubpicture;

struct _GstMfxSubpicture
{
  GstMfxSurface *surface;
  gfloat global_alpha;
  GstMfxRectangle sub_rect;
};

GstMfxSubpictureComposition *
gst_mfx_subpicture_composition_new (GstMfxDisplay * display,
  GstVideoOverlayComposition * overlay, gboolean memtype_is_system);

GstMfxSubpictureComposition *
gst_mfx_subpicture_composition_ref (GstMfxSubpictureComposition * composition);

void
gst_mfx_subpicture_composition_unref(GstMfxSubpictureComposition * composition);

void
gst_mfx_subpicture_composition_replace(
  GstMfxSubpictureComposition ** old_composition_ptr,
  GstMfxSubpictureComposition * new_composition);

void
gst_mfx_subpicture_composition_add_base_surface (GstMfxSubpictureComposition * composition,
  GstMfxSurface * surface);

GstMfxSurface *
gst_mfx_subpicture_composition_get_base_surface (GstMfxSubpictureComposition * composition);

guint
gst_mfx_subpicture_composition_get_num_subpictures(GstMfxSubpictureComposition * composition);

GstMfxSurface *
gst_mfx_subpicture_composition_get_subpicture(GstMfxSubpictureComposition * composition,
  guint index);

G_END_DECLS

#endif /* GST_MFX_SUBPICTURE_COMPOSITION_H */

