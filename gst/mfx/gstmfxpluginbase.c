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

#define DEBUG 1
#include "gstmfxdebug.h"

#ifdef HAVE_GST_GL_LIBS
#if GST_CHECK_VERSION(1,11,1)
# ifndef GST_GL_TYPE_CONTEXT
# define GST_GL_TYPE_CONTEXT GST_TYPE_GL_CONTEXT
# endif // GST_GL_TYPE_CONTEXT
#else
# ifdef WITH_LIBVA_BACKEND
# include <gst/gl/egl/gstglcontext_egl.h>
# endif // WITH_LIBVA_BACKEND
#endif // GST_CHECK_VERSION

#ifdef WITH_LIBVA_BACKEND
# include <EGL/egl.h>
# include <EGL/eglext.h>
# include <drm/drm_fourcc.h>

# include <gst/gl/egl/gstgldisplay_egl.h>

typedef void *GLeglImageOES;
typedef void(*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target,
    GLeglImageOES image);

PFNEGLCREATEIMAGEKHRPROC egl_create_image_khr;
PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image_khr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gl_egl_image_target_texture2d_oes;

# else
# ifdef WITH_D3D11_BACKEND
# include <GL/wglext.h>

PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV = NULL;
PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV = NULL;
PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV = NULL;
PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV = NULL;
PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV = NULL;
PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV = NULL;
PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV = NULL;

# endif
# endif // WITH_LIBVA_BACKEND
#endif // HAVE_GST_GL_LIBS

static gpointer plugin_parent_class = NULL;

#ifdef WITH_LIBVA_BACKEND
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
    }
    else if (GST_IS_BASE_TRANSFORM (element)) {
      if (sscanf (element_name, "capsfilter%d", &v) != 1)
        break;

      pad = gst_element_get_static_pad (element, "sink");
      if (!pad)
        break;
    }
    else
      break;

    g_free (element_name);
    element_name = NULL;
    g_clear_object (&element);
  }

  g_free (element_name);
  g_clear_object (&element);
  return is_dmabuf_capable;
}
#endif // WITH_LIBVA_BACKEND

#ifdef HAVE_GST_GL_LIBS
#ifdef WITH_D3D11_BACKEND
/* call from GL thread */
static void
clean_dxgl_interop (GstGLContext * context, HANDLE dxgl_handle)
{
  wglDXCloseDeviceNV (dxgl_handle);
}

/* call from GL thread */
static void
ensure_dxgl_interop (GstGLContext * context, gpointer * args)
{
  ID3D11Device* d3d11_device = args[0];
  HANDLE* dxgl_device_out = args[1];

  wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)
    gst_gl_context_get_proc_address(context, "wglDXOpenDeviceNV");
  wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)
    gst_gl_context_get_proc_address(context, "wglDXCloseDeviceNV");
  wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)
    gst_gl_context_get_proc_address(context, "wglDXRegisterObjectNV");
  wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)
    gst_gl_context_get_proc_address(context, "wglDXUnregisterObjectNV");
  wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)
    gst_gl_context_get_proc_address(context, "wglDXSetResourceShareHandleNV");
  wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)
    gst_gl_context_get_proc_address(context, "wglDXLockObjectsNV");
  wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)
    gst_gl_context_get_proc_address(context, "wglDXUnlockObjectsNV");

  *dxgl_device_out = wglDXOpenDeviceNV (d3d11_device);
}
#else
static void
ensure_egl_dmabuf (GstGLContext * context, gpointer * args)
{

  gl_egl_image_target_texture2d_oes = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
    gst_gl_context_get_proc_address (context, "glEGLImageTargetTexture2DOES");
  egl_create_image_khr = (PFNEGLCREATEIMAGEKHRPROC)
    gst_gl_context_get_proc_address (context, "eglCreateImageKHR");
  egl_destroy_image_khr = (PFNEGLDESTROYIMAGEKHRPROC)
    gst_gl_context_get_proc_address (context, "eglDestroyImageKHR");
}
#endif // WITH_D3D11_BACKEND
#endif // HAVE_GST_GL_LIBS

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

void
gst_mfx_plugin_base_class_init (GstMfxPluginBaseClass * klass)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
    "mfxtaskaggregator", 0, "MFX Context");

  plugin_parent_class = g_type_class_peek_parent (klass);

  GstElementClass *const element_class = GST_ELEMENT_CLASS (klass);
  element_class->set_context = GST_DEBUG_FUNCPTR (plugin_set_aggregator);
}

void
gst_mfx_plugin_base_init (GstMfxPluginBase * plugin,
  GstDebugCategory * debug_category)
{
  plugin->debug_category = debug_category;

  GST_DEBUG_CATEGORY_INIT (debug_category,
    "mfxpluginbase", 0, "MFX Context");

  /* sink pad */
  plugin->sinkpad = gst_element_get_static_pad (GST_ELEMENT (plugin), "sink");
  gst_video_info_init (&plugin->sinkpad_info);

  /* src pad */
  if (!(GST_OBJECT_FLAGS (plugin) & GST_ELEMENT_FLAG_SINK))
    plugin->srcpad = gst_element_get_static_pad (GST_ELEMENT (plugin), "src");
  gst_video_info_init (&plugin->srcpad_info);
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
#ifdef HAVE_GST_GL_LIBS
  if (plugin->gl_context) {
#ifdef WITH_D3D11_BACKEND
    if (plugin->gl_context_dxgl_handle) {
      gst_gl_context_thread_add (plugin->gl_context,
        (GstGLContextThreadFunc) clean_dxgl_interop,
        plugin->gl_context_dxgl_handle);

      plugin->gl_context_dxgl_handle = NULL;
    }
#endif
    gst_object_replace ((GstObject **) &plugin->gl_context, NULL);
  }
#endif

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
}

gboolean
gst_mfx_plugin_base_ensure_aggregator (GstMfxPluginBase * plugin)
{
  gst_mfx_task_aggregator_replace (&plugin->aggregator, NULL);

  return gst_mfx_ensure_aggregator (GST_ELEMENT (plugin));
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

#ifdef WITH_LIBVA_BACKEND
  if (!plugin->sinkpad_buffer_pool)
    plugin->sinkpad_has_dmabuf =
        has_dmabuf_capable_peer (plugin, plugin->sinkpad);
#endif // WITH_LIBVA_BACKEND

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

  pool = gst_mfx_video_buffer_pool_new (plugin->aggregator,
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

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (need_pool) {
    if (!caps) {
      GST_INFO_OBJECT(plugin, "no caps specified");
      return FALSE;
    }
    if (!ensure_sinkpad_buffer_pool (plugin, caps))
      return FALSE;
    gst_query_add_allocation_pool (query, plugin->sinkpad_buffer_pool,
      plugin->sinkpad_buffer_size, 0, 0);
#ifdef WITH_LIBVA_BACKEND
    if (plugin->sinkpad_has_dmabuf) {
      GstStructure *const config =
        gst_buffer_pool_get_config (plugin->sinkpad_buffer_pool);

      gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_DMABUF_MEMORY);
      if (!gst_buffer_pool_set_config (plugin->sinkpad_buffer_pool, config)) {
        GST_ERROR_OBJECT(plugin, "failed to reset buffer pool config");
        return FALSE;
      }
    }
#endif // WITH_LIBVA_BACKEND
  }

  gst_query_add_allocation_meta (query, GST_MFX_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
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
#ifdef HAVE_GST_GL_LIBS
  guint idx;
  GstGLContext *gl_context = NULL;
  GstStructure *params = NULL;
#endif

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps)
    goto error_no_caps;

  has_video_meta = gst_query_find_allocation_meta (query,
    GST_VIDEO_META_API_TYPE, NULL);

  /* Kept in case the GL context could not be retrieved via
   * gst_gl_query_local_gl_context() which was only introduced since 1.11.2 */
#if !GST_CHECK_VERSION(1,11,2)
#ifdef HAVE_GST_GL_LIBS
  if (!plugin->gl_context && gst_query_find_allocation_meta (query,
        GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, &idx)) {
    gst_query_parse_nth_allocation_meta (query, idx, &params);
    if (params) {
      if (gst_structure_get (params, "gst.gl.GstGLContext", GST_GL_TYPE_CONTEXT,
            &gl_context, NULL) && gl_context) {
#ifdef WITH_LIBVA_BACKEND
#if !GST_CHECK_VERSION(1,11,1)
        plugin->can_export_gl_textures =
          (GST_IS_GL_CONTEXT_EGL (gl_context) &&
            !(gst_gl_context_get_gl_api (gl_context) & GST_GL_API_GLES1)
            && gst_gl_check_extension ("EGL_EXT_image_dma_buf_import",
                GST_GL_CONTEXT_EGL (gl_context)->egl_exts));
#else
        plugin->can_export_gl_textures =
          !(gst_gl_context_get_gl_api (gl_context) & GST_GL_API_GLES1)
          && gst_gl_context_check_feature (gl_context,
              "EGL_EXT_image_dma_buf_import");
#endif //GST_CHECK_VERSION
#else
        plugin->can_export_gl_textures =
          gst_gl_context_check_feature (gl_context, "GL_INTEL_map_texture");
#endif // WITH_LIBVA_BACKEND
        plugin->can_export_gl_textures &= gst_caps_has_gl_memory (caps);

        gst_object_replace ((GstObject **) &plugin->gl_context,
          GST_OBJECT (gl_context));
        gst_object_unref (gl_context);
      }
    }
  }
#endif // HAVE_GST_GL_LIBS
#endif //GST_CHECK_VERSION

  if (!gst_mfx_plugin_base_ensure_aggregator (plugin))
    goto error_ensure_aggregator;

  gst_video_info_init (&vi);
  gst_video_info_from_caps (&vi, caps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
    size = MAX (size, vi.size);
  }
  else {
    pool = NULL;
    size = vi.size;
    min = max = 0;
  }

#ifdef HAVE_GST_GL_LIBS
  if (plugin->can_export_gl_textures) {
#ifdef WITH_LIBVA_BACKEND
    gst_gl_context_thread_add (plugin->gl_context,
      (GstGLContextThreadFunc) ensure_egl_dmabuf, NULL);
#else
    GstMfxContext *context =
      gst_mfx_task_aggregator_get_context (plugin->aggregator);
    gpointer args[2];

    args[0] = (ID3D11Device*)
      gst_mfx_d3d11_device_get_handle (gst_mfx_context_get_device (context));
    args[1] = &(plugin->gl_context_dxgl_handle);

    gst_mfx_context_unref (context);

    gst_gl_context_thread_add (plugin->gl_context,
      (GstGLContextThreadFunc) ensure_dxgl_interop, args);
#endif //WITH_LIBVA_BACKEND
    if (pool)
      gst_object_unref (pool);
    pool = gst_gl_buffer_pool_new (plugin->gl_context);
    if (!pool)
      goto error_create_pool;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, min, max);
    gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }
  else
#endif
    /* GstMfxVideoMeta is mandatory, and this implies VA surface memory */
    if (!pool || !gst_buffer_pool_has_option (pool,
      GST_BUFFER_POOL_OPTION_MFX_VIDEO_META)) {
      GST_INFO_OBJECT (plugin, "%s. Making a new pool", pool == NULL ? "No pool" :
        "Pool not configured with GstMfxVideoMeta option");
      if (pool)
        gst_object_unref (pool);
      pool = gst_mfx_video_buffer_pool_new (plugin->aggregator,
        plugin->srcpad_caps_is_raw);
      if (!pool)
        goto error_create_pool;

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, caps, size, min, max);
      gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_MFX_VIDEO_META);
      if (!gst_buffer_pool_set_config (pool, config))
        goto config_failed;

      /* Check whether GstVideoMeta, or GstVideoAlignment, is needed (raw video) */
      if (has_video_meta) {
        if (!gst_mfx_plugin_base_set_pool_config (pool,
          GST_BUFFER_POOL_OPTION_VIDEO_META))
          goto config_failed;
      }
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

#ifdef HAVE_GST_GL_LIBS
#ifdef WITH_LIBVA_BACKEND

typedef struct _GstMfxEglDmaBufInfo
{
  GstMfxSurface *surface;
  GstGLContext *gl_context;
  EGLDisplay egl_display;
  EGLImageKHR egl_image;
  GLuint gl_texture_id;
} GstMfxEglDmaBufInfo;

/* call from GL thread */
static void
create_egl_objects (GstGLContext * context, gpointer * args)
{
  GstMfxEglDmaBufInfo *egl_info = (GstMfxEglDmaBufInfo *) args[0];
  GstGLMemory *gl_mem = GST_GL_MEMORY_CAST (args[1]);

  if (gst_mfx_surface_has_video_memory (egl_info->surface)) {
    GLint attribs[13], *attrib;
    const GstMfxRectangle *crop_rect;
    GstMfxPrimeBufferProxy *buffer_proxy;
    VaapiImage *image;

    buffer_proxy =
        gst_mfx_prime_buffer_proxy_new_from_surface (egl_info->surface);
    if (!buffer_proxy)
      return;

    image = gst_mfx_prime_buffer_proxy_get_vaapi_image (buffer_proxy);

    attrib = attribs;
    *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
    *attrib++ = DRM_FORMAT_ARGB8888;
    *attrib++ = EGL_WIDTH;
    *attrib++ = gst_gl_memory_get_texture_width (gl_mem);
    *attrib++ = EGL_HEIGHT;
    *attrib++ = gst_gl_memory_get_texture_height (gl_mem);
    *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
    *attrib++ = GST_MFX_PRIME_BUFFER_PROXY_HANDLE (buffer_proxy);
    *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    *attrib++ = vaapi_image_get_offset (image, 0);
    *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    *attrib++ = vaapi_image_get_pitch (image, 0);
    *attrib++ = EGL_NONE;

#if GST_CHECK_VERSION(1,11,1)
    GstGLDisplay *display = gst_gl_context_get_display (egl_info->gl_context);
    egl_info->egl_display = gst_gl_display_egl_get_from_native (
      gst_gl_display_get_handle_type (display),
      gst_gl_display_get_handle (display));
    gst_object_unref (display);
#else
    egl_info->egl_display =
        GST_GL_CONTEXT_EGL (egl_info->gl_context)->egl_display;
#endif

    egl_info->egl_image =
        egl_create_image_khr (egl_info->egl_display, EGL_NO_CONTEXT,
          EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer) NULL, attribs);
    if (!egl_info->egl_image) {
      GST_WARNING ("failed to import dmabuf (RGBA) into EGL image");
      goto done;
    }

    egl_info->gl_texture_id = gst_gl_memory_get_texture_id (gl_mem);

    glBindTexture (GL_TEXTURE_2D, egl_info->gl_texture_id);
    gl_egl_image_target_texture2d_oes (GL_TEXTURE_2D, egl_info->egl_image);
    glBindTexture (GL_TEXTURE_2D, 0);

done:
    vaapi_image_unref (image);
    gst_mfx_prime_buffer_proxy_unref (buffer_proxy);
  }
  else {
    gst_gl_memory_read_pixels (gl_mem,
      (gpointer) gst_mfx_surface_get_plane (egl_info->surface, 0));
  }
}

/* call from GL thread */
static void
destroy_egl_objects (GstGLContext * context,
  gpointer * args)
{
  egl_destroy_image_khr (args[0], args[1]);
}

static void
egl_dmabuf_info_unref (GstMfxEglDmaBufInfo * egl_info)
{
  gpointer args[2];
  args[0] = egl_info->egl_display;
  args[1] = egl_info->egl_image;

  if (gst_mfx_surface_has_video_memory (egl_info->surface))
    gst_gl_context_thread_add (egl_info->gl_context,
      (GstGLContextThreadFunc) destroy_egl_objects, args);

  gst_object_unref (egl_info->gl_context);
  gst_mfx_surface_unref (egl_info->surface);
  g_slice_free (GstMfxEglDmaBufInfo, egl_info);
}

gboolean
gst_mfx_plugin_base_export_surface_to_gl (GstMfxPluginBase * plugin,
  GstMfxSurface * surface, GstBuffer * outbuf)
{
  GstMfxEglDmaBufInfo *egl_dmabuf_info = NULL;
  GstMemory *mem = gst_buffer_peek_memory (outbuf, 0);
  gpointer args[2];

  g_return_val_if_fail (outbuf && GST_IS_BUFFER (outbuf)
    && plugin->can_export_gl_textures
    && plugin->gl_context
    && gst_is_gl_base_memory (mem), FALSE);

  egl_dmabuf_info= g_slice_new (GstMfxEglDmaBufInfo);
  if (!egl_dmabuf_info) {
    GST_WARNING ("Failed to allocate GstMfxDXGLInteropInfo");
    return FALSE;
  }

  egl_dmabuf_info->gl_context =
      gst_object_ref (GST_OBJECT (plugin->gl_context));
  egl_dmabuf_info->surface = gst_mfx_surface_ref (surface);

  args[0] = egl_dmabuf_info;
  args[1] = mem;

  gst_gl_context_thread_add (egl_dmabuf_info->gl_context,
    (GstGLContextThreadFunc) create_egl_objects, args);

  gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
    g_quark_from_static_string ("GstMfxEglDmaBufInfo"), egl_dmabuf_info,
    (GDestroyNotify) egl_dmabuf_info_unref);

  return TRUE;
}
#else

typedef struct _GstMfxDXGLInteropInfo
{
  GstMfxSurface *surface;
  GstGLContext *gl_context;
  ID3D11Texture2D *d3d_texture;
  HANDLE *dxgl_handle;
  GLuint gl_texture_id;
  HANDLE gl_texture_handle; //from wglDXRegisterObjectNV
} GstMfxDXGLInteropInfo;

/* call from GL thread */
static void
lock_dxgl_interop (GstGLContext * context, gpointer * args)
{
  GstMfxDXGLInteropInfo *interop_info = (GstMfxDXGLInteropInfo*) args[0];
  GstGLMemory *gl_mem = GST_GL_MEMORY_CAST (args[1]);

  if (gst_mfx_surface_has_video_memory (interop_info->surface)) {
    interop_info->d3d_texture =
      (ID3D11Texture2D*) gst_mfx_surface_get_id (interop_info->surface);
    interop_info->gl_texture_id = gst_gl_memory_get_texture_id (gl_mem);

    interop_info->gl_texture_handle = wglDXRegisterObjectNV (
      interop_info->dxgl_handle,
      interop_info->d3d_texture,
      interop_info->gl_texture_id,
      GL_TEXTURE_2D,
      WGL_ACCESS_READ_ONLY_NV);

    if (!interop_info->gl_texture_handle) {
      GST_WARNING("couldn't get GL texture handle from DX texture %p",
        interop_info->d3d_texture);
    }

    if (!wglDXLockObjectsNV (interop_info->dxgl_handle,
            1, &(interop_info->gl_texture_handle))) {
      GST_WARNING("couldn't lock GL texture %p",
        interop_info->gl_texture_handle);
    }
  }
  else {
    gst_gl_memory_read_pixels (gl_mem,
      (gpointer) gst_mfx_surface_get_plane (interop_info->surface, 0));
  }
}

/* call from GL thread */
static void
unlock_dxgl_interop (GstGLContext * context,
  gpointer * args)
{
  if (wglDXUnlockObjectsNV (args[0], 1, &(args[1])))
    GST_DEBUG ("unlocked texture %u", args[2]);
  wglDXUnregisterObjectNV (args[0], args[1]);
}

static void
dxgl_interop_info_unref (GstMfxDXGLInteropInfo * interop_info)
{
  gpointer args[3];
  args[0] = interop_info->dxgl_handle;
  args[1] = interop_info->gl_texture_handle;
  args[2] = interop_info->gl_texture_id;

  if (gst_mfx_surface_has_video_memory (interop_info->surface))
    gst_gl_context_thread_add (interop_info->gl_context,
      (GstGLContextThreadFunc) unlock_dxgl_interop, args);

  gst_object_unref (interop_info->gl_context);
  gst_mfx_surface_unref (interop_info->surface);
  g_slice_free (GstMfxDXGLInteropInfo, interop_info);
}

gboolean
gst_mfx_plugin_base_export_surface_to_gl (GstMfxPluginBase * plugin,
  GstMfxSurface * surface, GstBuffer * outbuf)
{
  GstMfxDXGLInteropInfo *dxgl_interop_info = NULL;
  GstMemory *mem = gst_buffer_peek_memory (outbuf, 0);
  gpointer args[2];

  g_return_val_if_fail (outbuf && GST_IS_BUFFER (outbuf)
    && plugin->can_export_gl_textures
    && plugin->gl_context
    && gst_is_gl_base_memory (mem), FALSE);

  dxgl_interop_info = g_slice_new (GstMfxDXGLInteropInfo);
  if (!dxgl_interop_info) {
    GST_WARNING ("Failed to allocate GstMfxDXGLInteropInfo");
    return FALSE;
  }

  dxgl_interop_info->gl_context =
      gst_object_ref (GST_OBJECT (plugin->gl_context));
  dxgl_interop_info->dxgl_handle = plugin->gl_context_dxgl_handle;
  dxgl_interop_info->surface = gst_mfx_surface_ref (surface);

  args[0] = dxgl_interop_info;
  args[1] = mem;

  gst_gl_context_thread_add (dxgl_interop_info->gl_context,
    (GstGLContextThreadFunc) lock_dxgl_interop, args);

  gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
    g_quark_from_static_string ("GstMfxDXGLInteropInfo"), dxgl_interop_info,
    (GDestroyNotify) dxgl_interop_info_unref);

  return TRUE;
}
#endif // WITH_LIBVA_BACKEND
#endif // HAVE_GST_GL_LIBS
