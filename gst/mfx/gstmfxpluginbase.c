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

#include <gst/base/gstpushsrc.h>
#include <gst/allocators/allocators.h>

#include "gstmfxpluginbase.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideocontext.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideobufferpool.h"

#ifdef HAVE_GST_GL_LIBS
# if GST_CHECK_VERSION(1,11,1)
# include <gst/gl/gstglcontext.h>
# else
# include <gst/gl/egl/gstglcontext_egl.h>
# endif
#endif

/* Default debug category is from the subclass */
#define GST_CAT_DEFAULT (plugin->debug_category)

static gpointer plugin_parent_class = NULL;

static void
plugin_set_aggregator (GstElement * element, GstContext * context)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (element);
  GstElementClass *element_class = GST_ELEMENT_CLASS (plugin_parent_class);
  GstMfxTaskAggregator *aggregator = NULL;

  if (gst_mfx_video_context_get_aggregator (context, &aggregator)) {
    gst_mfx_task_aggregator_replace (&plugin->aggregator, aggregator);
    gst_mfx_task_aggregator_unref (aggregator);
  }

  if (element_class->set_context)
    element_class->set_context (element, context);
}

void
gst_mfx_plugin_base_init_interfaces (GType g_define_type_id)
{
}

static gboolean
default_has_interface (GstMfxPluginBase * plugin, GType type)
{
  return FALSE;
}

void
gst_mfx_plugin_base_class_init (GstMfxPluginBaseClass * klass)
{
  klass->has_interface = default_has_interface;

  plugin_parent_class = g_type_class_peek_parent (klass);

  GstElementClass *const element_class = GST_ELEMENT_CLASS (klass);
  element_class->set_context = GST_DEBUG_FUNCPTR (plugin_set_aggregator);
}

void
gst_mfx_plugin_base_init (GstMfxPluginBase * plugin,
    GstDebugCategory * debug_category)
{
  plugin->debug_category = debug_category;

  /* sink pad */
  plugin->sinkpad = gst_element_get_static_pad (GST_ELEMENT (plugin), "sink");
  gst_video_info_init (&plugin->sinkpad_info);
  plugin->sinkpad_query = GST_PAD_QUERYFUNC (plugin->sinkpad);

  /* src pad */
  if (!(GST_OBJECT_FLAGS (plugin) & GST_ELEMENT_FLAG_SINK)) {
    plugin->srcpad = gst_element_get_static_pad (GST_ELEMENT (plugin), "src");
    plugin->srcpad_query = GST_PAD_QUERYFUNC (plugin->srcpad);
  }
  gst_video_info_init (&plugin->srcpad_info);

  plugin->need_linear_dmabuf = FALSE;
}

void
gst_mfx_plugin_base_finalize (GstMfxPluginBase * plugin)
{
  gst_mfx_plugin_base_close (plugin);
  if (plugin->sinkpad)
    gst_object_unref (plugin->sinkpad);
  if (plugin->srcpad)
    gst_object_unref (plugin->srcpad);
}

/**
 * gst_mfx_plugin_base_close:
 * @plugin: a #GstMfxPluginBase
 *
 * Deallocates all internal resources that were allocated so
 * far. i.e. put the base plugin object into a clean state.
 */
void
gst_mfx_plugin_base_close (GstMfxPluginBase * plugin)
{
  gst_mfx_task_aggregator_replace (&plugin->aggregator, NULL);

  gst_caps_replace (&plugin->sinkpad_caps, NULL);
  plugin->sinkpad_caps_changed = FALSE;
  gst_video_info_init (&plugin->sinkpad_info);
  if (plugin->sinkpad_buffer_pool) {
    gst_object_unref (plugin->sinkpad_buffer_pool);
    plugin->sinkpad_buffer_pool = NULL;
  }
  g_clear_object (&plugin->srcpad_buffer_pool);

  gst_caps_replace (&plugin->srcpad_caps, NULL);
  plugin->srcpad_caps_changed = FALSE;
  gst_video_info_init (&plugin->srcpad_info);
  plugin->need_linear_dmabuf = FALSE;
}

gboolean
gst_mfx_plugin_base_ensure_aggregator (GstMfxPluginBase * plugin)
{
  gst_mfx_task_aggregator_replace (&plugin->aggregator, NULL);

  return gst_mfx_ensure_aggregator (GST_ELEMENT (plugin));
}

/* Checks whether the supplied pad peer element supports DMABUF sharing */
/* XXX: this is a workaround to the absence of any proposer way to
specify DMABUF memory capsfeatures or bufferpool option to downstream */
static gboolean
has_dmabuf_capable_peer (GstMfxPluginBase * plugin, GstPad * pad)
{
  GstPad *other_pad = NULL;
  GstElement *element = NULL;
  gchar *element_name = NULL;
  gboolean is_dmabuf_capable = FALSE;
  gint v;

  gst_object_ref (pad);

  for (;;) {
    other_pad = gst_pad_get_peer (pad);
    gst_object_unref (pad);
    if (!other_pad)
      break;

    element = gst_pad_get_parent_element (other_pad);
    gst_object_unref (other_pad);
    if (!element)
      break;

    element_name = gst_element_get_name (element);
    if (!element_name)
      break;

    if (strstr (element_name, "v4l2src")
        || strstr (element_name, "camerasrc")) {
      g_object_get (element, "io-mode", &v, NULL);
      if (strncmp (element_name, "camerasrc", 9) == 0)
        is_dmabuf_capable = v == 3;
      else
        is_dmabuf_capable = v == 5;     /* "dmabuf-import" enum value */
      break;
    } else if (GST_IS_BASE_TRANSFORM (element)) {
      if (sscanf (element_name, "capsfilter%d", &v) != 1)
        break;

      pad = gst_element_get_static_pad (element, "sink");
      if (!pad)
        break;
    } else
      break;

    g_free (element_name);
    element_name = NULL;
    g_clear_object (&element);
  }

  g_free (element_name);
  g_clear_object (&element);
  return is_dmabuf_capable;
}

/**
 * ensure_sinkpad_buffer_pool:
 * @plugin: a #GstMfxPluginBase
 * @caps: the initial #GstCaps for the resulting buffer pool
 *
 * Makes sure the sink pad video buffer pool is created with the
 * appropriate @caps.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
static gboolean
ensure_sinkpad_buffer_pool (GstMfxPluginBase * plugin, GstCaps * caps)
{
  GstBufferPool *pool;
  GstCaps *pool_caps;
  GstStructure *config;
  GstVideoInfo vi;
  gboolean need_pool;

  if (!plugin->sinkpad_buffer_pool)
    plugin->sinkpad_has_dmabuf =
        has_dmabuf_capable_peer (plugin, plugin->sinkpad);

  plugin->sinkpad_caps_is_raw = !plugin->sinkpad_has_dmabuf &&
      !gst_caps_has_mfx_surface (caps);

  if (!gst_mfx_plugin_base_ensure_aggregator (plugin))
    return FALSE;

  if (plugin->sinkpad_buffer_pool) {
    config = gst_buffer_pool_get_config (plugin->sinkpad_buffer_pool);
    gst_buffer_pool_config_get_params (config, &pool_caps, NULL, NULL, NULL);
    need_pool = !gst_caps_is_equal (caps, pool_caps);
    gst_structure_free (config);
    if (!need_pool)
      return TRUE;
    g_clear_object (&plugin->sinkpad_buffer_pool);
    plugin->sinkpad_buffer_size = 0;
  }

  pool =
      gst_mfx_video_buffer_pool_new (plugin->aggregator,
        plugin->sinkpad_caps_is_raw);
  if (!pool)
    goto error_create_pool;

  gst_video_info_init (&vi);
  gst_video_info_from_caps (&vi, caps);
  plugin->sinkpad_buffer_size = vi.size;

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps,
      plugin->sinkpad_buffer_size, 0, 0);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_MFX_VIDEO_META);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (!gst_buffer_pool_set_config (pool, config))
    goto error_pool_config;
  plugin->sinkpad_buffer_pool = pool;

  return TRUE;

  /* ERRORS */
error_create_pool:
  {
    GST_ERROR ("failed to create buffer pool");
    return FALSE;
  }
error_pool_config:
  {
    GST_ERROR ("failed to reset buffer pool config");
    gst_object_unref (pool);
    return FALSE;
  }
}

/**
 * gst_mfx_plugin_base_set_caps:
 * @plugin: a #GstMfxPluginBase
 * @incaps: the sink pad (input) caps
 * @outcaps: the src pad (output) caps
 *
 * Notifies the base plugin object of the new input and output caps,
 * obtained from the subclass.
 *
 * Returns: %TRUE if the update of caps was successful, %FALSE otherwise.
 */
gboolean
gst_mfx_plugin_base_set_caps (GstMfxPluginBase * plugin, GstCaps * incaps,
    GstCaps * outcaps)
{
  if (outcaps && outcaps != plugin->srcpad_caps) {
    gst_caps_replace (&plugin->srcpad_caps, outcaps);
    if (!gst_video_info_from_caps (&plugin->srcpad_info, outcaps))
      return FALSE;
    plugin->srcpad_caps_changed = TRUE;
  }

  if (incaps && incaps != plugin->sinkpad_caps) {
    gst_caps_replace (&plugin->sinkpad_caps, incaps);
    if (!gst_video_info_from_caps (&plugin->sinkpad_info, incaps))
      return FALSE;
    plugin->sinkpad_caps_changed = TRUE;
    plugin->sinkpad_caps_is_raw = !gst_caps_has_mfx_surface (incaps);
  }

  if (!GST_IS_VIDEO_DECODER (plugin))
    if (!ensure_sinkpad_buffer_pool (plugin, plugin->sinkpad_caps))
      return FALSE;

  return TRUE;
}

/**
 * gst_mfx_plugin_base_propose_allocation:
 * @plugin: a #GstMfxPluginBase
 * @query: the allocation query to configure
 *
 * Proposes allocation parameters to the upstream elements.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gst_mfx_plugin_base_propose_allocation (GstMfxPluginBase * plugin,
    GstQuery * query)
{
  GstCaps *caps = NULL;
  gboolean need_pool;
  gboolean tiled = TRUE;
  GstStructure *structure = NULL;
  guint num = 0;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (need_pool) {
    if (!caps)
      goto error_no_caps;

    num = gst_caps_get_size (caps);
    for (guint i=0; i < num; i++) {
      structure = gst_caps_get_structure (caps, i);
      if (gst_structure_has_field (structure, "tiled")) {
        gst_structure_get_boolean (structure, "tiled", &tiled);
        plugin->need_linear_dmabuf = !(tiled);

        if (plugin->need_linear_dmabuf) {
          gst_mfx_video_buffer_pool_set_untiled (plugin->sinkpad_buffer_pool,
          plugin->need_linear_dmabuf);
        }
      }
    }

    if (!ensure_sinkpad_buffer_pool (plugin, caps))
      return FALSE;
    gst_query_add_allocation_pool (query, plugin->sinkpad_buffer_pool,
        plugin->sinkpad_buffer_size, 0, 0);

    if (plugin->sinkpad_has_dmabuf) {
      GstStructure *const config =
          gst_buffer_pool_get_config (plugin->sinkpad_buffer_pool);

      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_DMABUF_MEMORY);
      if (!gst_buffer_pool_set_config (plugin->sinkpad_buffer_pool, config))
        goto error_pool_config;
    }
  }

  gst_query_add_allocation_meta (query, GST_MFX_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
/* ERRORS */
error_no_caps:
  {
    GST_INFO_OBJECT (plugin, "no caps specified");
    return FALSE;
  }
error_pool_config:
  {
    GST_ERROR_OBJECT (plugin, "failed to reset buffer pool config");
    return FALSE;
  }
}

static inline gboolean
gst_mfx_plugin_base_set_pool_config (GstBufferPool * pool, const gchar * option)
{
  GstStructure *config;

  config = gst_buffer_pool_get_config (pool);
  if (!gst_buffer_pool_config_has_option (config, option)) {
    gst_buffer_pool_config_add_option (config, option);
    return gst_buffer_pool_set_config (pool, config);
  }
  gst_structure_free (config);
  return TRUE;
}

/**
 * gst_mfx_plugin_base_decide_allocation:
 * @plugin: a #GstMfxPluginBase
 * @query: the allocation query to parse
 * @feature: the desired #GstMfxCapsFeature, or zero to find the
 *   preferred one
 *
 * Decides allocation parameters for the downstream elements.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gst_mfx_plugin_base_decide_allocation (GstMfxPluginBase * plugin,
    GstQuery * query)
{
  GstCaps *caps = NULL;
  GstBufferPool *pool;
  GstStructure *config;
  GstVideoInfo vi;
  guint size, min, max;
  gboolean update_pool = FALSE;
  gboolean has_video_meta = FALSE;
  const GstStructure *params;
  guint idx;

  g_return_val_if_fail (plugin->aggregator != NULL, FALSE);

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps)
    goto error_no_caps;

  has_video_meta = gst_query_find_allocation_meta (query,
      GST_VIDEO_META_API_TYPE, NULL);

#if GST_CHECK_VERSION(1,8,0)
#ifdef HAVE_GST_GL_LIBS
  if (gst_query_find_allocation_meta(query,
      GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, &idx)) {
    gst_query_parse_nth_allocation_meta (query, idx, &params);
    if (params) {
      GstGLContext *gl_context;
# if GST_CHECK_VERSION(1,11,1)
      if (gst_structure_get (params, "gst.gl.GstGLContext", GST_TYPE_GL_CONTEXT,
          &gl_context, NULL) && gl_context) {
        plugin->srcpad_has_dmabuf =
            !(gst_gl_context_get_gl_api (gl_context) & GST_GL_API_GLES1) &&
            gst_gl_context_check_feature (gl_context, "EGL_EXT_image_dma_buf_import");
        gst_object_unref (gl_context);
      }
# else
      if (gst_structure_get (params, "gst.gl.GstGLContext", GST_GL_TYPE_CONTEXT,
          &gl_context, NULL) && gl_context) {
        plugin->srcpad_has_dmabuf =
            (GST_IS_GL_CONTEXT_EGL (gl_context) &&
            !(gst_gl_context_get_gl_api (gl_context) & GST_GL_API_GLES1) &&
            gst_gl_check_extension ("EGL_EXT_image_dma_buf_import",
              GST_GL_CONTEXT_EGL (gl_context)->egl_exts));
        gst_object_unref (gl_context);
      }
# endif
    }
  }
#endif
#endif

  if (!plugin->srcpad_has_dmabuf)
    plugin->srcpad_caps_is_raw = !gst_caps_has_mfx_surface (caps);

  if (!gst_mfx_plugin_base_ensure_aggregator (plugin))
    goto error_ensure_aggregator;

  gst_video_info_init (&vi);
  gst_video_info_from_caps (&vi, caps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
    size = MAX (size, vi.size);
  } else {
    pool = NULL;
    size = vi.size;
    min = max = 0;
  }

  /* GstMfxVideoMeta is mandatory, and this implies VA surface memory */
  if (!pool || !gst_buffer_pool_has_option (pool,
          GST_BUFFER_POOL_OPTION_MFX_VIDEO_META)) {
    GST_INFO_OBJECT (plugin, "%s. Making a new pool", pool == NULL ? "No pool" :
        "Pool not configured with GstMfxVideoMeta option");
    if (pool)
      gst_object_unref (pool);
    pool =
        gst_mfx_video_buffer_pool_new (plugin->aggregator,
          plugin->srcpad_caps_is_raw);
    if (!pool)
      goto error_create_pool;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, min, max);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_MFX_VIDEO_META);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }

  /* Check whether GstVideoMeta, or GstVideoAlignment, is needed (raw video) */
  if (has_video_meta) {
    if (!gst_mfx_plugin_base_set_pool_config (pool,
            GST_BUFFER_POOL_OPTION_VIDEO_META))
      goto config_failed;
  }

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  g_clear_object (&plugin->srcpad_buffer_pool);
  plugin->srcpad_buffer_pool = pool;
  return TRUE;

  /* ERRORS */
error_no_caps:
  {
    GST_ERROR_OBJECT (plugin, "no caps specified");
    return FALSE;
  }
error_ensure_aggregator:
  {
    GST_ERROR_OBJECT (plugin, "failed to ensure aggregator");
    return FALSE;
  }
error_create_pool:
  {
    GST_ERROR_OBJECT (plugin, "failed to create buffer pool");
    return FALSE;
  }
config_failed:
  {
    if (pool)
      gst_object_unref (pool);
    GST_ELEMENT_ERROR (plugin, RESOURCE, SETTINGS,
        ("Failed to configure the buffer pool"),
        ("Configuration is most likely invalid, please report this issue."));
    return FALSE;
  }
}

/**
 * gst_mfx_plugin_base_get_input_buffer:
 * @plugin: a #GstMfxPluginBase
 * @incaps: the sink pad (input) buffer
 * @outbuf_ptr: the pointer to location to the VA surface backed buffer
 *
 * Acquires the sink pad (input) buffer as a VA surface backed
 * buffer. This is mostly useful for raw YUV buffers, as source
 * buffers that are already backed as a VA surface are passed
 * verbatim.
 *
 * Returns: #GST_FLOW_OK if the buffer could be acquired
 */
GstFlowReturn
gst_mfx_plugin_base_get_input_buffer (GstMfxPluginBase * plugin,
    GstBuffer * inbuf, GstBuffer ** outbuf_ptr)
{
  GstMfxVideoMeta *meta;
  GstBuffer *outbuf;
  GstVideoFrame src_frame, out_frame;
  gboolean success;

  g_return_val_if_fail (inbuf != NULL, GST_FLOW_ERROR);
  g_return_val_if_fail (outbuf_ptr != NULL, GST_FLOW_ERROR);

  meta = gst_buffer_get_mfx_video_meta (inbuf);
  if (meta) {
    *outbuf_ptr = gst_buffer_ref (inbuf);
    return GST_FLOW_OK;
  }

  if (!plugin->sinkpad_caps_is_raw)
    goto error_invalid_buffer;

  if (!plugin->sinkpad_buffer_pool)
    goto error_no_pool;

  if (!gst_buffer_pool_set_active (plugin->sinkpad_buffer_pool, TRUE))
    goto error_active_pool;

  outbuf = NULL;
  if (gst_buffer_pool_acquire_buffer (plugin->sinkpad_buffer_pool,
        &outbuf, NULL) != GST_FLOW_OK)
    goto error_create_buffer;

  if (!gst_video_frame_map (&src_frame, &plugin->sinkpad_info, inbuf,
        GST_MAP_READ))
    goto error_map_src_buffer;

  if (!gst_video_frame_map (&out_frame, &plugin->sinkpad_info, outbuf,
        GST_MAP_WRITE))
    goto error_map_dst_buffer;

  /* Hack for incoming video frames with changed GstVideoInfo dimensions */
  GST_VIDEO_FRAME_WIDTH (&src_frame) = GST_VIDEO_FRAME_WIDTH (&out_frame);
  GST_VIDEO_FRAME_HEIGHT (&src_frame) = GST_VIDEO_FRAME_HEIGHT (&out_frame);

  success = gst_video_frame_copy (&out_frame, &src_frame);
  gst_video_frame_unmap (&out_frame);
  gst_video_frame_unmap (&src_frame);
  if (!success)
    goto error_copy_buffer;

  gst_buffer_copy_into (outbuf, inbuf,
    GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
  *outbuf_ptr = outbuf;
  return GST_FLOW_OK;

  /* ERRORS */
error_no_pool:
  {
    GST_ELEMENT_ERROR (plugin, STREAM, FAILED,
        ("no buffer pool was negotiated"), ("no buffer pool was negotiated"));
    return GST_FLOW_ERROR;
  }
error_active_pool:
  {
    GST_ELEMENT_ERROR (plugin, STREAM, FAILED,
        ("failed to activate buffer pool"), ("failed to activate buffer pool"));
    return GST_FLOW_ERROR;
  }
error_map_dst_buffer:
  {
    gst_video_frame_unmap (&src_frame);
    // fall-through
  }
error_map_src_buffer:
  {
    GST_WARNING ("failed to map buffer");
    gst_buffer_unref (outbuf);
    return GST_FLOW_NOT_SUPPORTED;
  }

  /* ERRORS */
error_invalid_buffer:
  {
    GST_ELEMENT_ERROR (plugin, STREAM, FAILED,
        ("failed to validate source buffer"),
        ("failed to validate source buffer"));
    return GST_FLOW_ERROR;
  }
error_create_buffer:
  {
    GST_ELEMENT_ERROR (plugin, STREAM, FAILED, ("Allocation failed"),
        ("failed to create buffer"));
    return GST_FLOW_ERROR;
  }
error_copy_buffer:
  {
    GST_WARNING ("failed to upload buffer to VA surface");
    gst_buffer_unref (outbuf);
    return GST_FLOW_NOT_SUPPORTED;
  }
}

#if GST_CHECK_VERSION(1,8,0)
gboolean
gst_mfx_plugin_base_export_dma_buffer (GstMfxPluginBase * plugin,
    GstBuffer * outbuf)
{
  GstMfxVideoMeta *vmeta;
  GstVideoMeta *meta = gst_buffer_get_video_meta (outbuf);
  GstMfxSurface *surface;
  GstMfxPrimeBufferProxy *dmabuf_proxy;
  GstMemory *mem;
  VaapiImage *image;
  GstBuffer *buf;
  guint i;

  g_return_val_if_fail (outbuf && GST_IS_BUFFER (outbuf), FALSE);

  if (!plugin->srcpad_has_dmabuf)
    return FALSE;

  vmeta = gst_buffer_get_mfx_video_meta (outbuf);
  if (!vmeta)
    return FALSE;
  surface = gst_mfx_video_meta_get_surface (vmeta);
  if (!surface || !gst_mfx_surface_has_video_memory(surface))
    return FALSE;

  dmabuf_proxy = gst_mfx_prime_buffer_proxy_new_from_surface (surface);
  if (!dmabuf_proxy)
    return FALSE;

  if (!plugin->dmabuf_allocator)
    plugin->dmabuf_allocator = gst_dmabuf_allocator_new ();

  mem = gst_dmabuf_allocator_alloc (plugin->dmabuf_allocator,
      gst_mfx_prime_buffer_proxy_get_handle (dmabuf_proxy),
      gst_mfx_prime_buffer_proxy_get_size (dmabuf_proxy));
  if (!mem)
    goto error_dmabuf_handle;

  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem),
      g_quark_from_static_string ("GstMfxPrimeBufferProxy"), dmabuf_proxy,
      (GDestroyNotify) gst_mfx_prime_buffer_proxy_unref);

  buf = gst_buffer_copy(outbuf);
  gst_buffer_prepend_memory (buf, gst_buffer_get_memory (outbuf, 0));
  gst_buffer_add_parent_buffer_meta (outbuf, buf);
  gst_buffer_replace_memory (outbuf, 0, mem);

  gst_buffer_unref (buf);

  image = gst_mfx_prime_buffer_proxy_get_vaapi_image (dmabuf_proxy);
  for (i = 0; i < vaapi_image_get_plane_count (image); i++) {
    meta->offset[i] = vaapi_image_get_offset (image, i);
    meta->stride[i] = vaapi_image_get_pitch (image, i);
  }

  vaapi_image_unref(image);
  return TRUE;
  /* ERRORS */
error_dmabuf_handle:
  {
    gst_mfx_prime_buffer_proxy_unref (dmabuf_proxy);
    return FALSE;
  }
}
#endif
