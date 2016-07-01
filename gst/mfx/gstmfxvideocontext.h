/*
 *  Copyright (C) 2011-2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2013 Igalia
 *    Author: Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
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

#ifndef GST_MFX_VIDEO_CONTEXT_H
#define GST_MFX_VIDEO_CONTEXT_H

#include <gst-libs/mfx/gstmfxtaskaggregator.h>

#define GST_MFX_AGGREGATOR_CONTEXT_TYPE_NAME "gst.mfx.Aggregator"

void
gst_mfx_video_context_set_aggregator(GstContext * context,
    GstMfxTaskAggregator * aggregator);

GstContext *
gst_mfx_video_context_new_with_aggregator(GstMfxTaskAggregator * aggregator,
    gboolean persistent);

gboolean
gst_mfx_video_context_get_aggregator(GstContext * context,
    GstMfxTaskAggregator ** aggregator_ptr);

gboolean
gst_mfx_video_context_prepare(GstElement * element,
    GstMfxTaskAggregator ** aggregator_ptr);

void
gst_mfx_video_context_propagate(GstElement * element,
    GstMfxTaskAggregator * aggregator);

#endif /* GST_MFX_VIDEO_CONTEXT_H */
