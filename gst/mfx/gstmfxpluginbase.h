/*
 *  Copyright (C) 2011-2013 Intel Corporation
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

#ifndef GST_MFX_PLUGIN_BASE_H
#define GST_MFX_PLUGIN_BASE_H

#include <gst/base/gstbasetransform.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/video/gstvideoencoder.h>
#include <gst/video/gstvideosink.h>

#ifdef HAVE_GST_GL_LIBS
# include <gst/gl/gstglcontext.h>

# if GST_CHECK_VERSION(1,13,0)
# include <gst/gl/gstglfuncs.h>
# include <gst/gl/gstglmemory.h>
# include <gst/gl/gstglbufferpool.h>
# endif

#endif // HAVE_GST_GL_LIBS

#include <gst-libs/mfx/gstmfxtaskaggregator.h>
#include <gst-libs/mfx/gstmfxsurface.h>

G_BEGIN_DECLS typedef struct _GstMfxPluginBase GstMfxPluginBase;
typedef struct _GstMfxPluginBaseClass GstMfxPluginBaseClass;

#define GST_MFX_PLUGIN_BASE(plugin) \
  ((GstMfxPluginBase *)(plugin))
#define GST_MFX_PLUGIN_BASE_CLASS(plugin) \
  ((GstMfxPluginBaseClass *)(plugin))
#define GST_MFX_PLUGIN_BASE_GET_CLASS(plugin) \
  GST_MFX_PLUGIN_BASE_CLASS(GST_ELEMENT_GET_CLASS ( \
  GST_MFX_PLUGIN_BASE_ELEMENT(plugin)))
#define GST_MFX_PLUGIN_BASE_PARENT(plugin) \
  (&GST_MFX_PLUGIN_BASE(plugin)->parent_instance)
#define GST_MFX_PLUGIN_BASE_PARENT_CLASS(plugin) \
  (&GST_MFX_PLUGIN_BASE_CLASS(plugin)->parent_class)
#define GST_MFX_PLUGIN_BASE_ELEMENT(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT(plugin)->element)
#define GST_MFX_PLUGIN_BASE_ELEMENT_CLASS(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT_CLASS(plugin)->element)
#define GST_MFX_PLUGIN_BASE_DECODER(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT(plugin)->decoder)
#define GST_MFX_PLUGIN_BASE_DECODER_CLASS(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT_CLASS(plugin)->decoder)
#define GST_MFX_PLUGIN_BASE_ENCODER(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT (plugin)->encoder)
#define GST_MFX_PLUGIN_BASE_ENCODER_CLASS(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT_CLASS(plugin)->encoder)
#define GST_MFX_PLUGIN_BASE_TRANSFORM(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT(plugin)->transform)
#define GST_MFX_PLUGIN_BASE_TRANSFORM_CLASS(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT_CLASS(plugin)->transform)
#define GST_MFX_PLUGIN_BASE_SINK(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT(plugin)->sink)
#define GST_MFX_PLUGIN_BASE_SINK_CLASS(plugin) \
  (&GST_MFX_PLUGIN_BASE_PARENT_CLASS(plugin)->sink)

#define GST_MFX_PLUGIN_BASE_INIT_INTERFACES \
  gst_mfx_plugin_base_init_interfaces (g_define_type_id);

#define GST_MFX_PLUGIN_BASE_SINK_PAD(plugin) \
  (GST_MFX_PLUGIN_BASE(plugin)->sinkpad)
#define GST_MFX_PLUGIN_BASE_SINK_PAD_CAPS (plugin) \
  (GST_MFX_PLUGIN_BASE(plugin)->sinkpad_caps)
#define GST_MFX_PLUGIN_BASE_SINK_PAD_INFO(plugin) \
  (&GST_MFX_PLUGIN_BASE(plugin)->sinkpad_info)
#define GST_MFX_PLUGIN_BASE_SRC_PAD(plugin) \
  (GST_MFX_PLUGIN_BASE(plugin)->srcpad)
#define GST_MFX_PLUGIN_BASE_SRC_PAD_CAPS(plugin) \
  (GST_MFX_PLUGIN_BASE(plugin)->srcpad_caps)
#define GST_MFX_PLUGIN_BASE_SRC_PAD_INFO(plugin) \
  (&GST_MFX_PLUGIN_BASE(plugin)->srcpad_info)

#define GST_MFX_PLUGIN_BASE_AGGREGATOR(plugin) \
  (GST_MFX_PLUGIN_BASE(plugin)->aggregator)

struct _GstMfxPluginBase
{
  /*< private > */
  union
  {
    GstElement element;
    GstVideoDecoder decoder;
    GstVideoEncoder encoder;
    GstBaseTransform transform;
    GstVideoSink sink;
  } parent_instance;

  GstDebugCategory *debug_category;

  GstPad *sinkpad;
  GstCaps *sinkpad_caps;
  gboolean sinkpad_caps_changed;
  gboolean sinkpad_caps_is_raw;
  GstVideoInfo sinkpad_info;
  GstBufferPool *sinkpad_buffer_pool;
  guint sinkpad_buffer_size;

  GstPad *srcpad;
  GstCaps *srcpad_caps;
  gboolean srcpad_caps_changed;
  gboolean srcpad_caps_is_raw;
  GstVideoInfo srcpad_info;
  GstBufferPool *srcpad_buffer_pool;

  gboolean sinkpad_has_dmabuf;
  gboolean can_export_gl_textures;

#ifdef HAVE_GST_GL_LIBS
  GstGLContext *gl_context;
#ifdef WITH_D3D11_BACKEND
  gboolean sinkpad_has_dxgl_interop;    //TODO
  HANDLE gl_context_dxgl_handle;
#endif                          // WITH_D3D11_BACKEND
#endif

  GstMfxTaskAggregator *aggregator;
};

struct _GstMfxPluginBaseClass
{
  /*< private > */
  union
  {
    GstElementClass element;
    GstVideoDecoderClass decoder;
    GstVideoEncoderClass encoder;
    GstBaseTransformClass transform;
    GstVideoSinkClass sink;
  } parent_class;

    gboolean (*has_interface) (GstMfxPluginBase * plugin, GType type);
};

void
gst_mfx_plugin_base_init_interfaces (GType type);

void
gst_mfx_plugin_base_class_init (GstMfxPluginBaseClass * klass);

void
gst_mfx_plugin_base_init (GstMfxPluginBase * plugin,
    GstDebugCategory * debug_category);

void
gst_mfx_plugin_base_finalize (GstMfxPluginBase * plugin);

void
gst_mfx_plugin_base_close (GstMfxPluginBase * plugin);

gboolean
gst_mfx_plugin_base_ensure_aggregator (GstMfxPluginBase * plugin);

gboolean
gst_mfx_plugin_base_set_caps (GstMfxPluginBase * plugin, GstCaps * incaps,
    GstCaps * outcaps);

gboolean
gst_mfx_plugin_base_propose_allocation (GstMfxPluginBase * plugin,
    GstQuery * query);

gboolean
gst_mfx_plugin_base_decide_allocation (GstMfxPluginBase * plugin,
    GstQuery * query);

GstFlowReturn
gst_mfx_plugin_base_get_input_buffer (GstMfxPluginBase * plugin,
    GstBuffer * inbuf, GstBuffer ** outbuf_ptr);

gboolean
gst_mfx_plugin_base_export_surface_to_gl (GstMfxPluginBase * plugin,
    GstMfxSurface * surface, GstBuffer * outbuf);

G_END_DECLS
#endif /* GST_MFX_PLUGIN_BASE_H */
