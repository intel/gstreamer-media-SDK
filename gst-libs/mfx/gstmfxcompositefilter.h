/*
 *  gstmfxcompositefilter.h - MFX composite filter abstraction
 *
 *  Copyright (C) 2017 Intel Corporation
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

#ifndef GST_MFX_COMPOSITE_FILTER_H
#define GST_MFX_COMPOSITE_FILTER_H

#include <gst-libs/mfx/gstmfxtaskaggregator.h>
#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst-libs/mfx/gstmfxsurfacecomposition.h>

G_BEGIN_DECLS

#define GST_TYPE_MFX_COMPOSITE_FILTER (gst_mfx_composite_filter_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxCompositeFilter, gst_mfx_composite_filter, GST_MFX,
    COMPOSITE_FILTER, GstObject)

GstMfxCompositeFilter *
gst_mfx_composite_filter_new (GstMfxTaskAggregator * aggregator,
    gboolean memtype_is_system);

GstMfxCompositeFilter *
gst_mfx_composite_filter_ref (GstMfxCompositeFilter * filter);

void
gst_mfx_composite_filter_unref (GstMfxCompositeFilter * filter);

void
gst_mfx_composite_filter_replace (GstMfxCompositeFilter ** old_filter_ptr,
    GstMfxCompositeFilter * new_filter);

gboolean
gst_mfx_composite_filter_apply_composition (GstMfxCompositeFilter * filter,
    GstMfxSurfaceComposition * composition, GstMfxSurface ** out_surface);

G_END_DECLS
#endif /* GST_MFX_COMPOSITE_FILTER_H */
