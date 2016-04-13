#include <gst/base/gstpushsrc.h>
#include <gst/allocators/allocators.h>

#include "gstmfxpluginbase.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideocontext.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideobufferpool.h"

/* Default debug category is from the subclass */
#define GST_CAT_DEFAULT (plugin->debug_category)

static gpointer plugin_parent_class = NULL;

static void
plugin_set_aggregator(GstElement * element, GstContext * context)
{
	GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE(element);
	GstElementClass *element_class = GST_ELEMENT_CLASS(plugin_parent_class);
	GstMfxTaskAggregator *aggregator = NULL;

	if (gst_mfx_video_context_get_aggregator(context, &aggregator))
        gst_mfx_task_aggregator_replace(&plugin->aggregator, aggregator);

	if (element_class->set_context)
		element_class->set_context(element, context);
}

void
gst_mfx_plugin_base_init_interfaces(GType g_define_type_id)
{
}

static gboolean
default_has_interface(GstMfxPluginBase * plugin, GType type)
{
	return FALSE;
}

static gboolean
plugin_update_sinkpad_info_from_buffer(GstMfxPluginBase * plugin,
    GstBuffer * buf)
{
	GstVideoInfo *const vip = &plugin->sinkpad_info;
	GstVideoMeta *vmeta;
	guint i;

	vmeta = gst_buffer_get_video_meta(buf);
	if (!vmeta)
		return TRUE;

	if (GST_VIDEO_INFO_FORMAT(vip) != vmeta->format ||
		GST_VIDEO_INFO_WIDTH(vip) != vmeta->width ||
		GST_VIDEO_INFO_HEIGHT(vip) != vmeta->height ||
		GST_VIDEO_INFO_N_PLANES(vip) != vmeta->n_planes)
		return FALSE;

	for (i = 0; i < GST_VIDEO_INFO_N_PLANES(vip); ++i) {
		GST_VIDEO_INFO_PLANE_OFFSET(vip, i) = vmeta->offset[i];
		GST_VIDEO_INFO_PLANE_STRIDE(vip, i) = vmeta->stride[i];
	}
	GST_VIDEO_INFO_SIZE(vip) = gst_buffer_get_size(buf);
	return TRUE;
}


void
gst_mfx_plugin_base_class_init(GstMfxPluginBaseClass * klass)
{
	klass->has_interface = default_has_interface;

	plugin_parent_class = g_type_class_peek_parent(klass);

	GstElementClass *const element_class = GST_ELEMENT_CLASS(klass);
	element_class->set_context = GST_DEBUG_FUNCPTR(plugin_set_aggregator);
}

void
gst_mfx_plugin_base_init(GstMfxPluginBase * plugin,
	GstDebugCategory * debug_category)
{
	plugin->debug_category = debug_category;

	/* sink pad */
	plugin->sinkpad = gst_element_get_static_pad(GST_ELEMENT(plugin), "sink");
	gst_video_info_init(&plugin->sinkpad_info);
	plugin->sinkpad_query = GST_PAD_QUERYFUNC(plugin->sinkpad);

	/* src pad */
	if (!(GST_OBJECT_FLAGS(plugin) & GST_ELEMENT_FLAG_SINK)) {
		plugin->srcpad = gst_element_get_static_pad(GST_ELEMENT(plugin), "src");
		plugin->srcpad_query = GST_PAD_QUERYFUNC(plugin->srcpad);
	}
	gst_video_info_init(&plugin->srcpad_info);
}

void
gst_mfx_plugin_base_finalize(GstMfxPluginBase * plugin)
{
	gst_mfx_plugin_base_close(plugin);
	if (plugin->sinkpad)
		gst_object_unref(plugin->sinkpad);
	if (plugin->srcpad)
		gst_object_unref(plugin->srcpad);
}

/**
* gst_mfx_plugin_base_open:
* @plugin: a #GstMfxPluginBase
*
* Allocates any internal resources needed for correct operation from
* the subclass.
*
* Returns: %TRUE if successful, %FALSE otherwise.
*/
gboolean
gst_mfx_plugin_base_open(GstMfxPluginBase * plugin)
{
	return TRUE;
}

/**
* gst_mfx_plugin_base_close:
* @plugin: a #GstMfxPluginBase
*
* Deallocates all internal resources that were allocated so
* far. i.e. put the base plugin object into a clean state.
*/
void
gst_mfx_plugin_base_close(GstMfxPluginBase * plugin)
{
	gst_mfx_task_aggregator_replace(&plugin->aggregator, NULL);

	gst_caps_replace(&plugin->sinkpad_caps, NULL);
	plugin->sinkpad_caps_changed = FALSE;
	gst_video_info_init(&plugin->sinkpad_info);

	g_clear_object(&plugin->srcpad_buffer_pool);

	gst_caps_replace(&plugin->srcpad_caps, NULL);
	plugin->srcpad_caps_changed = FALSE;
	gst_video_info_init(&plugin->srcpad_info);
	gst_caps_replace(&plugin->allowed_raw_caps, NULL);
}

/**
* gst_mfx_plugin_base_ensure_display:
* @plugin: a #GstMfxPluginBase
*
* Ensures the display stored in @plugin complies with the requested
* display type constraints.
*
* Returns: %TRUE if the display was created to match the requested
*   type, %FALSE otherwise.
*/
gboolean
gst_mfx_plugin_base_ensure_aggregator(GstMfxPluginBase * plugin)
{
	gst_mfx_task_aggregator_replace(&plugin->aggregator, NULL);

	if (!gst_mfx_ensure_aggregator(GST_ELEMENT(plugin)))
		return FALSE;

	return TRUE;
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
gst_mfx_plugin_base_set_caps(GstMfxPluginBase * plugin, GstCaps * incaps,
    GstCaps * outcaps)
{
	if (incaps && incaps != plugin->sinkpad_caps) {
		gst_caps_replace(&plugin->sinkpad_caps, incaps);
		if (!gst_video_info_from_caps(&plugin->sinkpad_info, incaps))
			return FALSE;
		plugin->sinkpad_caps_changed = TRUE;
		plugin->sinkpad_caps_is_raw = !gst_caps_has_mfx_surface(incaps);
	}

	if (outcaps && outcaps != plugin->srcpad_caps) {
		gst_caps_replace(&plugin->srcpad_caps, outcaps);
		if (!gst_video_info_from_caps(&plugin->srcpad_info, outcaps))
			return FALSE;
		plugin->srcpad_caps_changed = TRUE;
	}

	return TRUE;
}

/* XXXX: GStreamer 1.2 doesn't check, in gst_buffer_pool_set_config()
if the config option is already set */
static inline gboolean
gst_mfx_plugin_base_set_pool_config(GstBufferPool * pool,
const gchar * option)
{
	GstStructure *config;

	config = gst_buffer_pool_get_config(pool);
	if (!gst_buffer_pool_config_has_option(config, option)) {
		gst_buffer_pool_config_add_option(config, option);
		return gst_buffer_pool_set_config(pool, config);
	}
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
gst_mfx_plugin_base_decide_allocation(GstMfxPluginBase * plugin,
	GstQuery * query)
{
	GstCaps *caps = NULL;
	GstBufferPool *pool;
	GstStructure *config;
	GstVideoInfo vi;
	guint size, min, max;
	gboolean update_pool = FALSE;
	gboolean has_video_meta = FALSE;
	gboolean has_video_alignment = FALSE;

	g_return_val_if_fail(plugin->aggregator != NULL, FALSE);

	gst_query_parse_allocation(query, &caps, NULL);

	if (!caps)
		goto error_no_caps;

	has_video_meta = gst_query_find_allocation_meta(query,
		GST_VIDEO_META_API_TYPE, NULL);

	/* Make sure the display we pass down to the buffer pool is actually
	the expected one, especially when the downstream element requires
	a GLX or EGL display */
	if (!gst_mfx_plugin_base_ensure_aggregator(plugin))
		goto error_ensure_aggregator;

	gst_video_info_init(&vi);
	gst_video_info_from_caps(&vi, caps);

	if (gst_query_get_n_allocation_pools(query) > 0) {
		gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
		update_pool = TRUE;
		size = MAX(size, vi.size);
		if (pool) {
			/* Check whether downstream element proposed a bufferpool but did
			not provide a correct propose_allocation() implementation */
			has_video_alignment = gst_buffer_pool_has_option(pool,
				GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
		}
	}
	else {
		pool = NULL;
		size = vi.size;
		min = max = 0;
	}

	/* GstMfxVideoMeta is mandatory, and this implies VA surface memory */
	if (!pool || !gst_buffer_pool_has_option(pool,
		GST_BUFFER_POOL_OPTION_MFX_VIDEO_META)) {
		GST_INFO_OBJECT(plugin, "%s. Making a new pool", pool == NULL ? "No pool" :
			"Pool not configured with GstMfxVideoMeta option");
		if (pool)
			gst_object_unref(pool);
		pool = gst_mfx_video_buffer_pool_new(
                    GST_MFX_TASK_AGGREGATOR_DISPLAY(plugin->aggregator));
		if (!pool)
			goto error_create_pool;

		config = gst_buffer_pool_get_config(pool);
		gst_buffer_pool_config_set_params(config, caps, size, min, max);
		gst_buffer_pool_config_add_option(config,
			GST_BUFFER_POOL_OPTION_MFX_VIDEO_META);
		if (!gst_buffer_pool_set_config(pool, config))
			goto config_failed;
	}

	/* Check whether GstVideoMeta, or GstVideoAlignment, is needed (raw video) */
	if (has_video_meta) {
		if (!gst_mfx_plugin_base_set_pool_config(pool,
			GST_BUFFER_POOL_OPTION_VIDEO_META))
			goto config_failed;
	}
	else if (has_video_alignment) {
		if (!gst_mfx_plugin_base_set_pool_config(pool,
			GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT))
			goto config_failed;
	}

	if (update_pool)
		gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
	else
		gst_query_add_allocation_pool(query, pool, size, min, max);

	g_clear_object(&plugin->srcpad_buffer_pool);
	plugin->srcpad_buffer_pool = pool;
	return TRUE;

	/* ERRORS */
error_no_caps:
	{
		GST_ERROR_OBJECT(plugin, "no caps specified");
		return FALSE;
	}
error_ensure_aggregator:
	{
		GST_ERROR_OBJECT(plugin, "failed to ensure aggregator");
		return FALSE;
	}
error_create_pool:
	{
		GST_ERROR_OBJECT(plugin, "failed to create buffer pool");
		return FALSE;
	}
config_failed:
	{
		if (pool)
			gst_object_unref(pool);
		GST_ELEMENT_ERROR(plugin, RESOURCE, SETTINGS,
			("Failed to configure the buffer pool"),
			("Configuration is most likely invalid, please report this issue."));
		return FALSE;
	}
}
