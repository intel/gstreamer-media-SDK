/*
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2011 Collabora
 *    Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
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

#include "gst-libs/mfx/sysdeps.h"
#include "gstmfxvideocontext.h"
#include "gstmfxpluginutil.h"
#include "gstmfxpluginbase.h"

gboolean
gst_mfx_ensure_aggregator (GstElement * element)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (element);
  GstMfxTaskAggregator *aggregator;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  if (gst_mfx_video_context_prepare (element, &plugin->aggregator))
    return TRUE;

  aggregator = g_object_new(GST_TYPE_MFX_TASK_AGGREGATOR, NULL);
  if (!aggregator)
    return FALSE;

  gst_mfx_video_context_propagate (element, aggregator);
  gst_mfx_task_aggregator_unref (aggregator);
  return TRUE;
}

gboolean
gst_mfx_handle_context_query (GstQuery * query, GstMfxTaskAggregator * task)
{
  const gchar *type = NULL;
  GstContext *context, *old_context;

  g_return_val_if_fail (query != NULL, FALSE);

  if (!task)
    return FALSE;

  if (!gst_query_parse_context_type (query, &type))
    return FALSE;

  if (g_strcmp0 (type, GST_MFX_AGGREGATOR_CONTEXT_TYPE_NAME))
    return FALSE;

  gst_query_parse_context (query, &old_context);
  if (old_context) {
    context = gst_context_copy (old_context);
    gst_mfx_video_context_set_aggregator (context, task);
  } else {
    context = gst_mfx_video_context_new_with_aggregator (task, FALSE);
  }

  gst_query_set_context (query, context);
  gst_context_unref (context);

  return TRUE;
}

gboolean
gst_mfx_append_surface_caps (GstCaps * out_caps, GstCaps * in_caps)
{
  GstStructure *structure;
  const GValue *v_width, *v_height, *v_framerate, *v_par;
  guint i, n_structures;

  structure = gst_caps_get_structure (in_caps, 0);
  v_width = gst_structure_get_value (structure, "width");
  v_height = gst_structure_get_value (structure, "height");
  v_framerate = gst_structure_get_value (structure, "framerate");
  v_par = gst_structure_get_value (structure, "pixel-aspect-ratio");
  if (!v_width || !v_height)
    return FALSE;

  n_structures = gst_caps_get_size (out_caps);
  for (i = 0; i < n_structures; i++) {
    structure = gst_caps_get_structure (out_caps, i);
    gst_structure_set_value (structure, "width", v_width);
    gst_structure_set_value (structure, "height", v_height);
    if (v_framerate)
      gst_structure_set_value (structure, "framerate", v_framerate);
    if (v_par)
      gst_structure_set_value (structure, "pixel-aspect-ratio", v_par);
  }
  return TRUE;
}

gboolean
gst_mfx_value_set_format (GValue * value, GstVideoFormat format)
{
  const gchar *str;

  str = gst_video_format_to_string (format);
  if (!str)
    return FALSE;

  g_value_init (value, G_TYPE_STRING);
  g_value_set_string (value, str);
  return TRUE;
}

void
set_video_template_caps (GstCaps * caps)
{
  GstStructure *const structure = gst_caps_get_structure (caps, 0);

  gst_structure_set (structure,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
      "pixel-aspect-ratio", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
}

GstCaps *
gst_mfx_video_format_new_template_caps (GstVideoFormat format)
{
  GstCaps *caps;

  g_return_val_if_fail (format != GST_VIDEO_FORMAT_UNKNOWN, NULL);

  caps = gst_caps_new_empty_simple ("video/x-raw");
  if (!caps)
    return NULL;

  gst_caps_set_simple (caps,
      "format", G_TYPE_STRING, gst_video_format_to_string (format), NULL);
  set_video_template_caps (caps);

  return caps;
}

GstCaps *
gst_mfx_video_format_new_template_caps_with_features (GstVideoFormat format,
    const gchar * features_string)
{
  GstCaps *caps;

  GstCapsFeatures *const features =
      gst_caps_features_new (features_string, NULL);

  if (!features)
    return NULL;

  caps = gst_mfx_video_format_new_template_caps (format);
  if (!caps)
    return NULL;

  gst_caps_set_features (caps, 0, features);
  return caps;
}

GstMfxCapsFeature
gst_mfx_find_preferred_caps_feature (GstPad * pad,
    GstVideoFormat * out_format_ptr)
{
  GstMfxCapsFeature feature = GST_MFX_CAPS_FEATURE_SYSTEM_MEMORY;
  guint num_structures;
  GstCaps *caps = NULL;
  GstCaps *out_caps, *templ;
  GstStructure *structure;
  const gchar *format = NULL;

  templ = gst_pad_get_pad_template_caps (pad);
  out_caps = gst_caps_intersect_full (gst_pad_peer_query_caps (pad, templ),
      templ, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (templ);
  if (!out_caps) {
    feature = GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED;
    goto cleanup;
  }

  if (gst_caps_has_mfx_surface (out_caps))
    feature = GST_MFX_CAPS_FEATURE_MFX_SURFACE;

  num_structures = gst_caps_get_size (out_caps);
  structure =
      gst_structure_copy (gst_caps_get_structure (out_caps, num_structures - 1));
  if (!structure)
      goto cleanup;
  if (gst_structure_has_field (structure, "format"))
    gst_structure_fixate_field (structure, "format");
  format = gst_structure_get_string (structure, "format");
  *out_format_ptr = gst_video_format_from_string (format);
  gst_structure_free (structure);

cleanup:
  gst_caps_replace (&caps, NULL);
  gst_caps_replace (&out_caps, NULL);
  return feature;
}

const gchar *
gst_mfx_caps_feature_to_string (GstMfxCapsFeature feature)
{
  const gchar *str;

  switch (feature) {
    case GST_MFX_CAPS_FEATURE_SYSTEM_MEMORY:
      str = GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY;
      break;
    case GST_MFX_CAPS_FEATURE_MFX_SURFACE:
      str = GST_CAPS_FEATURE_MEMORY_MFX_SURFACE;
      break;
    default:
      str = NULL;
      break;
  }
  return str;
}

static gboolean
_gst_caps_has_feature (const GstCaps * caps, const gchar * feature)
{
  guint i;

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstCapsFeatures *const features = gst_caps_get_features (caps, i);

    if (gst_caps_features_contains (features, feature))
      return TRUE;
  }

  return FALSE;
}

/* Checks whether the supplied caps contain MFX surfaces */
gboolean
gst_caps_has_mfx_surface (GstCaps * caps)
{
  g_return_val_if_fail (caps != NULL, FALSE);

  return _gst_caps_has_feature (caps, GST_CAPS_FEATURE_MEMORY_MFX_SURFACE);
}


gboolean
gst_mfx_query_peer_has_raw_caps(GstPad * srcpad)
{
  GstCaps *caps = NULL;
  gboolean has_raw_caps = TRUE;
  
  caps = gst_pad_peer_query_caps(srcpad, NULL);
  if (!caps)
    return has_raw_caps;
  
  if (gst_caps_has_mfx_surface(caps)
#if GST_CHECK_VERSION(1,8,0)
    || (!g_strcmp0(getenv("GST_GL_PLATFORM"), "egl")
      && _gst_caps_has_feature(caps,
        GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META))
#endif
      )
    has_raw_caps = FALSE;
  
  gst_caps_unref(caps);
  return has_raw_caps;
}

void
gst_video_info_change_format (GstVideoInfo * vip, GstVideoFormat format,
    guint width, guint height)
{
  GstVideoInfo vi = *vip;

  gst_video_info_set_format (vip, format, width, height);

  vip->interlace_mode = vi.interlace_mode;
  vip->flags = vi.flags;
  vip->views = vi.views;
  vip->par_n = vi.par_n;
  vip->par_d = vi.par_d;
  vip->fps_n = vi.fps_n;
  vip->fps_d = vi.fps_d;
}
