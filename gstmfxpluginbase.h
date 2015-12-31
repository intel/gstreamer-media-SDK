#ifndef GST_MFX_PLUGIN_BASE_H
#define GST_MFX_PLUGIN_BASE_H

#include <gst/base/gstbasetransform.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/video/gstvideoencoder.h>
#include <gst/video/gstvideosink.h>
#include "gstmfxdisplay.h"
#include "gstmfxcontext.h"

#ifdef HAVE_GST_GL_GL_H
# include <gst/gl/gstglcontext.h>
#endif

G_BEGIN_DECLS

typedef struct _GstMfxPluginBase GstMfxPluginBase;
typedef struct _GstMfxPluginBaseClass GstMfxPluginBaseClass;

#define GST_MFX_PLUGIN_BASE(plugin) \
	((GstMfxPluginBase *)(plugin))
#define GST_MFX_PLUGIN_BASE_CLASS(plugin) \
	((GstMfxPluginBaseClass *)(plugin))
#define GST_MFX_PLUGIN_BASE_GET_CLASS(plugin) \
	GST_MFX_PLUGIN_BASE_CLASS(GST_ELEMENT_GET_CLASS( \
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
	(&GST_MFX_PLUGIN_BASE_PARENT(plugin)->encoder)
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
	gst_mfx_plugin_base_init_interfaces(g_define_type_id);

#define GST_MFX_PLUGIN_BASE_SINK_PAD(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->sinkpad)
#define GST_MFX_PLUGIN_BASE_SINK_PAD_CAPS(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->sinkpad_caps)
#define GST_MFX_PLUGIN_BASE_SINK_PAD_INFO(plugin) \
	(&GST_MFX_PLUGIN_BASE(plugin)->sinkpad_info)
#define GST_MFX_PLUGIN_BASE_SINK_PAD_QUERYFUNC(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->sinkpad_query)
#define GST_MFX_PLUGIN_BASE_SRC_PAD(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->srcpad)
#define GST_MFX_PLUGIN_BASE_SRC_PAD_CAPS(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->srcpad_caps)
#define GST_MFX_PLUGIN_BASE_SRC_PAD_INFO(plugin) \
	(&GST_MFX_PLUGIN_BASE(plugin)->srcpad_info)
#define GST_MFX_PLUGIN_BASE_SRC_PAD_QUERYFYNC(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->srcpad_query)

#define GST_MFX_PLUGIN_BASE_DISPLAY(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->display)
#define GST_MFX_PLUGIN_BASE_DISPLAY_TYPE(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->display_type)
#define GST_MFX_PLUGIN_BASE_DISPLAY_NAME(plugin) \
	(GST_MFX_PLUGIN_BASE(plugin)->display_name)
#define GST_MFX_PLUGIN_BASE_DISPLAY_REPLACE(plugin, new_display) \
	(gst_mfx_display_replace(&GST_MFX_PLUGIN_BASE_DISPLAY(plugin), \
	(new_display)))

struct _GstMfxPluginBase
{
	/*< private >*/
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
	GstVideoInfo srcpad_info;
	GstBufferPool *srcpad_buffer_pool;

#if !GST_CHECK_VERSION(1,4,0)
	GstPadQueryFunction srcpad_query;
	GstPadQueryFunction sinkpad_query;
#endif

	GstMfxContextAllocatorVaapi alloc_ctx;

	GstMfxDisplay *display;
	GstMfxDisplayType display_type;
	GstMfxDisplayType display_type_req;
	gchar *display_name;

	GstObject *gl_context;

	GstCaps *allowed_raw_caps;
};

struct _GstMfxPluginBaseClass
{
	/*< private >*/
	union
	{
		GstElementClass element;
		GstVideoDecoderClass decoder;
		GstVideoEncoderClass encoder;
		GstBaseTransformClass transform;
		GstVideoSinkClass sink;
	} parent_class;

	gboolean(*has_interface) (GstMfxPluginBase * plugin, GType type);
	void(*display_changed) (GstMfxPluginBase * plugin);
};

void
gst_mfx_plugin_base_init_interfaces(GType type);

void
gst_mfx_plugin_base_class_init(GstMfxPluginBaseClass * klass);

void
gst_mfx_plugin_base_init(GstMfxPluginBase * plugin,
	GstDebugCategory * debug_category);

void
gst_mfx_plugin_base_finalize(GstMfxPluginBase * plugin);

gboolean
gst_mfx_plugin_base_open(GstMfxPluginBase * plugin);

void
gst_mfx_plugin_base_close(GstMfxPluginBase * plugin);

gboolean
gst_mfx_plugin_base_has_display_type(GstMfxPluginBase * plugin,
	GstMfxDisplayType display_type_req);

void
gst_mfx_plugin_base_set_display_type(GstMfxPluginBase * plugin,
	GstMfxDisplayType display_type);

void
gst_mfx_plugin_base_set_display_name(GstMfxPluginBase * plugin,
	const gchar * display_name);

gboolean
gst_mfx_plugin_base_ensure_display(GstMfxPluginBase * plugin);

gboolean
gst_mfx_plugin_base_set_caps(GstMfxPluginBase * plugin, GstCaps * incaps,
	GstCaps * outcaps);

gboolean
gst_mfx_plugin_base_propose_allocation(GstMfxPluginBase * plugin,
	GstQuery * query);

gboolean
gst_mfx_plugin_base_decide_allocation(GstMfxPluginBase * plugin,
	GstQuery * query);

GstFlowReturn
gst_mfx_plugin_base_get_input_buffer(GstMfxPluginBase * plugin,
	GstBuffer * inbuf, GstBuffer ** outbuf_ptr);

void
gst_mfx_plugin_base_set_gl_context(GstMfxPluginBase * plugin,
	GstObject * object);

GstCaps *
gst_mfx_plugin_base_get_allowed_raw_caps(GstMfxPluginBase * plugin);

G_END_DECLS

#endif /* GST_MFX_PLUGIN_BASE_H */