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

  aggregator = gst_mfx_task_aggregator_new ();
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

static void
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

#ifdef HAVE_GST_GL_LIBS
gboolean
gst_mfx_check_gl_texture_sharing (GstElement * element,
    GstPad * pad, GstGLContext ** gl_context_ptr)
{
#if GST_CHECK_VERSION(1,11,2)
  GstCaps *out_caps, *templ = NULL;
  GstCaps *in_caps = NULL;
  gboolean has_gl_texture_sharing = FALSE;
  gboolean found_context = FALSE;
  const char caps_str[] =
      GST_MFX_MAKE_OUTPUT_SURFACE_CAPS ";"
      GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
      "{ RGBA }") ";";

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (gl_context_ptr != NULL, FALSE);

  if (*gl_context_ptr)
    return TRUE;

  templ = gst_caps_from_string (caps_str);
  in_caps = gst_pad_peer_query_caps (pad, templ);

  out_caps = gst_caps_intersect_full (templ, in_caps, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (templ);

  /* if peer caps can negotiate MFX surfaces, don't use GL sharing */
  if (!out_caps || gst_caps_has_mfx_surface (out_caps))
    goto done;

  found_context =
      gst_gl_query_local_gl_context (element, GST_PAD_SRC, gl_context_ptr);

  if (found_context) {
#ifdef WITH_LIBVA_BACKEND
    has_gl_texture_sharing =
        !(gst_gl_context_get_gl_api (*gl_context_ptr) & GST_GL_API_GLES1)
        && gst_gl_context_check_feature (*gl_context_ptr,
        "EGL_EXT_image_dma_buf_import");
#else
    /*
     * TODO: should ideally check for "WGL_NV_DX_interop2")
     * and that gl and dx are on the same device... but while WGL_NV_DX_interop2
     * is reported as available in debug, a check for it still returns 0
     * so instead we check for GL_INTEL_map_texture which is available
     * on all intel graphics since Sandy Bridge era.
     */
    has_gl_texture_sharing =
        gst_gl_context_check_feature (*gl_context_ptr, "GL_INTEL_map_texture");
#endif
  }

  /* Don't set a GL context if GL texture sharing is not supported */
  if (!has_gl_texture_sharing)
    *gl_context_ptr = NULL;

done:
  gst_caps_replace (&in_caps, NULL);
  gst_caps_replace (&out_caps, NULL);
  return has_gl_texture_sharing;
#else
  /* Assume that GL texture sharing is supported for versions < 1.11.2.
   * This support is to be validated later on in gst_mfx_base_decide_allocation() */
  return TRUE;
#endif //GST_CHECK_VERSION
}
#endif

GstMfxCapsFeature
gst_mfx_find_preferred_caps_feature (GstPad * pad,
    gboolean use_10bpc, gboolean has_gl_texture_sharing,
    GstVideoFormat * out_format_ptr)
{
  GstMfxCapsFeature feature = GST_MFX_CAPS_FEATURE_SYSTEM_MEMORY;
  guint num_structures;
  GstCaps *out_caps, *templ = NULL;
  GstCaps *in_caps = NULL;
  GstStructure *structure;
  const gchar *format = NULL;
  int i;

  /* Prefer 10-bit color format when requested */
  if (use_10bpc) {
#if GST_CHECK_VERSION(1,9,1)
    const char caps_str[] =
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_MFX_SURFACE,
        "{ P010_10LE, ENCODED, NV12, BGRA, YUY2 }") "; "
#ifdef HAVE_GST_GL_LIBS
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
        "{ RGBA }") ";"
#endif
        GST_VIDEO_CAPS_MAKE ("{ P010_10LE, NV12, BGRA, YUY2 }");
#else
    const char caps_str[] =
        GST_MFX_MAKE_OUTPUT_SURFACE_CAPS ";"
#ifdef HAVE_GST_GL_LIBS
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
        "{ RGBA }") ";"
#endif
        GST_VIDEO_CAPS_MAKE (GST_MFX_SUPPORTED_OUTPUT_FORMATS);
#endif
    templ = gst_caps_from_string (caps_str);
  } else {
    templ = gst_pad_get_pad_template_caps (pad);
  }
  in_caps = gst_pad_peer_query_caps (pad, templ);

  out_caps = gst_caps_intersect_full (templ, in_caps, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (templ);
  if (!out_caps) {
    feature = GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED;
    goto cleanup;
  }

  if (gst_caps_has_mfx_surface (out_caps)) {
    feature = GST_MFX_CAPS_FEATURE_MFX_SURFACE;
  }
#ifdef HAVE_GST_GL_LIBS
  else if (gst_caps_has_gl_memory (out_caps) && has_gl_texture_sharing) {
    feature = GST_MFX_CAPS_FEATURE_GL_MEMORY;
    *out_format_ptr = GST_VIDEO_FORMAT_RGBA;
    goto cleanup;
  }
#endif

  num_structures = gst_caps_get_size (out_caps);
  for (i = num_structures - 1; i >= 0; i--) {
    GstCapsFeatures *const features = gst_caps_get_features (out_caps, i);

    if (!gst_caps_features_contains (features,
            gst_mfx_caps_feature_to_string (feature)))
      continue;

    structure = gst_structure_copy (gst_caps_get_structure (out_caps, i));
    if (!structure)
      goto cleanup;
    if (gst_structure_has_field (structure, "format"))
      gst_structure_fixate_field (structure, "format");
    format = gst_structure_get_string (structure, "format");
    *out_format_ptr = gst_video_format_from_string (format);
    gst_structure_free (structure);

    break;
  }

cleanup:
  gst_caps_replace (&in_caps, NULL);
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
#ifdef HAVE_GST_GL_LIBS
    case GST_MFX_CAPS_FEATURE_GL_MEMORY:
      str = GST_CAPS_FEATURE_MEMORY_GL_MEMORY;
      break;
#endif
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

/* Checks whether the supplied caps contain GL surfaces */
gboolean
gst_caps_has_gl_memory (GstCaps * caps)
{
  g_return_val_if_fail (caps != NULL, FALSE);
#ifdef HAVE_GST_GL_LIBS
  return _gst_caps_has_feature (caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
#else
  return FALSE;
#endif
}

gboolean
gst_mfx_query_peer_task (GstElement * element, gint * task_id)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (element);
  GstStructure *structure =
      gst_structure_new ("task", "id", G_TYPE_INT, -1, NULL);
  gboolean ret = FALSE;

  if (structure) {
    GstQuery *custom_query =
        gst_query_new_custom (GST_QUERY_CUSTOM, structure);
    if (gst_pad_peer_query (GST_MFX_PLUGIN_BASE_SINK_PAD (plugin),
          custom_query)) {
      if (gst_structure_get_int (structure, "id", task_id))
        ret = TRUE;
    }
    gst_query_unref (custom_query);
  }

  return ret;
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

gboolean
gst_mfx_is_mfx_supported (mfxU16 * platform_code)
{
  mfxStatus sts = MFX_ERR_NONE;
  mfxSession session = NULL;
  mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;
  mfxVersion ver = { { GST_MFX_MIN_MSDK_VERSION_MINOR,
      GST_MFX_MIN_MSDK_VERSION_MAJOR }
  };

#ifdef WITH_D3D11_BACKEND
  impl |= MFX_IMPL_VIA_D3D11;
#endif // WITH_D3D11_BACKEND

  sts = MFXInit (impl, &ver, &session);
  if (sts != MFX_ERR_NONE) {
    return FALSE;
  }
#if MSDK_CHECK_VERSION(1,19)
  if (platform_code == NULL)
    goto cleanup;

  sts = MFXQueryVersion (session, &ver);
  if (sts != MFX_ERR_NONE) {
    GST_ERROR ("Error querying MFX version.");
    goto cleanup;
  }

  if ((ver.Major == 1 && ver.Minor >= 19) || ver.Major > 1) {
    mfxPlatform platform = { 0 };
#ifdef WITH_LIBVA_BACKEND
    GstMfxDisplay *display = gst_mfx_display_new ();
    if (!display) {
      GST_ERROR ("Failed to initialize display.");
      goto cleanup;
    }
    MFXVideoCORE_SetHandle (session, MFX_HANDLE_VA_DISPLAY,
      GST_MFX_DISPLAY_VADISPLAY (display));
#endif // WITH_LIBVA_BACKEND
    sts = MFXVideoCORE_QueryPlatform (session, &platform);
    if (MFX_ERR_NONE == sts) {
      *platform_code = platform.CodeName;
      GST_INFO ("Detected MFX platform with device code %d", *platform_code);
    }
    else {
      GST_WARNING ("Platform autodetection failed with MFX status %d", sts);
    }
#ifdef WITH_LIBVA_BACKEND
    gst_mfx_display_unref (display);
#endif // WITH_LIBVA_BACKEND
  }
cleanup:
#endif // MSDK_CHECK_VERSION
  MFXClose (session);
  return TRUE;
}
