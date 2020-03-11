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

#ifndef GST_MFX_PLUGIN_UTIL_H
#define GST_MFX_PLUGIN_UTIL_H

#include "gstmfxvideomemory.h"

#include <gst-libs/mfx/gstmfxtaskaggregator.h>
#include <gst-libs/mfx/gstmfxsurface.h>

gboolean
gst_mfx_ensure_aggregator(GstElement * element);

gboolean
gst_mfx_handle_context_query (GstQuery * query, GstMfxTaskAggregator * context);

gboolean
gst_mfx_append_surface_caps(GstCaps * out_caps, GstCaps * in_caps);

/* Helpers for GValue construction for video formats */
gboolean
gst_mfx_value_set_format(GValue * value, GstVideoFormat format);

/* Helpers to build video caps */
typedef enum
{
  GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED,
  GST_MFX_CAPS_FEATURE_SYSTEM_MEMORY,
  GST_MFX_CAPS_FEATURE_MFX_SURFACE,
} GstMfxCapsFeature;

GstCaps *
gst_mfx_video_format_new_template_caps(GstVideoFormat format);

GstCaps *
gst_mfx_video_format_new_template_caps_from_list(GArray * formats);

GstCaps *
gst_mfx_video_format_new_template_caps_with_features(GstVideoFormat format,
  const gchar * features_string);

gboolean
gst_mfx_search_incompatibility (GstElement * element);

GstMfxCapsFeature
gst_mfx_find_preferred_caps_feature(GstPad * pad,
    GstVideoFormat * out_format_ptr, gboolean insist_prefer);

const gchar *
gst_mfx_caps_feature_to_string(GstMfxCapsFeature feature);

/* Helpers to handle interlaced contents */
#define GST_CAPS_INTERLACED_MODES \
    "interlace-mode = (string){ progressive, interleaved, mixed }"

#define GST_MFX_MAKE_SURFACE_CAPS               \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES(          \
    GST_CAPS_FEATURE_MEMORY_MFX_SURFACE, "{ NV12, BGRA }")

#ifdef WITH_MSS_2016
#define GST_MFX_SUPPORTED_INPUT_FORMATS \
    "{ NV12, YV12, I420, YUY2, BGRA, BGRx }"
#else
#define GST_MFX_SUPPORTED_INPUT_FORMATS \
    "{ NV12, YV12, I420, UYVY, YUY2, BGRA, BGRx }"
#endif


gboolean
gst_caps_has_mfx_surface(GstCaps * caps);

gboolean
gst_mfx_query_peer_has_raw_caps(GstPad * pad);

void
gst_video_info_change_format(GstVideoInfo * vip, GstVideoFormat format,
    guint width, guint height);

#endif /* GST_MFX_PLUGIN_UTIL_H */
