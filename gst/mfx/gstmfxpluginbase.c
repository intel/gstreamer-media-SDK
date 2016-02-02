#include <gst/base/gstpushsrc.h>
#include "gstmfxpluginbase.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideocontext.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideobufferpool.h"
#include <gst/allocators/allocators.h>

/* Default debug category is from the subclass */
#define GST_CAT_DEFAULT (plugin->debug_category)

static gpointer plugin_parent_class = NULL;

/* GstVideoContext interface */
static void
plugin_set_display(GstMfxPluginBase * plugin, GstMfxDisplay * display)
{
	const gchar *const display_name =
		gst_mfx_display_get_display_name(display);

	if (plugin->display_name && g_strcmp0(plugin->display_name, display_name)) {
		GST_DEBUG_OBJECT(plugin, "incompatible display name '%s', requested '%s'",
			display_name, plugin->display_name);
		gst_mfx_display_replace(&plugin->display, NULL);
	}
	else {
		GST_INFO_OBJECT(plugin, "set display %p", display);
		gst_mfx_display_replace(&plugin->display, display);
		plugin->display_type = gst_mfx_display_get_display_type(display);
		gst_mfx_plugin_base_set_display_name(plugin, display_name);
	}
	gst_mfx_display_unref(display);
}

static void
plugin_set_context(GstElement * element, GstContext * context)
{
	GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE(element);
	GstElementClass *element_class = GST_ELEMENT_CLASS(plugin_parent_class);
	GstMfxDisplay *display = NULL;

	if (gst_mfx_video_context_get_display(context, &display))
		plugin_set_display(plugin, display);

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

static void
default_display_changed(GstMfxPluginBase * plugin)
{
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
	klass->display_changed = default_display_changed;

	plugin_parent_class = g_type_class_peek_parent(klass);

	GstElementClass *const element_class = GST_ELEMENT_CLASS(klass);
	element_class->set_context = GST_DEBUG_FUNCPTR(plugin_set_context);
}

void
gst_mfx_plugin_base_init(GstMfxPluginBase * plugin,
	GstDebugCategory * debug_category)
{
	plugin->debug_category = debug_category;
	plugin->display_type = GST_MFX_DISPLAY_TYPE_ANY;
	plugin->display_type_req = GST_MFX_DISPLAY_TYPE_ANY;

	/* sink pad */
	plugin->sinkpad = gst_element_get_static_pad(GST_ELEMENT(plugin), "sink");
	gst_video_info_init(&plugin->sinkpad_info);
#if !GST_CHECK_VERSION(1,4,0)
	plugin->sinkpad_query = GST_PAD_QUERYFUNC(plugin->sinkpad);
#endif

	/* src pad */
	if (!(GST_OBJECT_FLAGS(plugin) & GST_ELEMENT_FLAG_SINK)) {
		plugin->srcpad = gst_element_get_static_pad(GST_ELEMENT(plugin), "src");
#if !GST_CHECK_VERSION(1,4,0)
		plugin->srcpad_query = GST_PAD_QUERYFUNC(plugin->srcpad);
#endif
	}
	gst_video_info_init(&plugin->srcpad_info);
}

void
gst_mfx_plugin_base_finalize(GstMfxPluginBase * plugin)
{
	gst_mfx_plugin_base_close(plugin);
	g_free(plugin->display_name);
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
	gst_mfx_display_replace(&plugin->display, NULL);
	//gst_object_replace(&plugin->gl_context, NULL);

	gst_caps_replace(&plugin->sinkpad_caps, NULL);
	plugin->sinkpad_caps_changed = FALSE;
	gst_video_info_init(&plugin->sinkpad_info);
	if (plugin->sinkpad_buffer_pool) {
		gst_object_unref(plugin->sinkpad_buffer_pool);
		plugin->sinkpad_buffer_pool = NULL;
	}
	g_clear_object(&plugin->srcpad_buffer_pool);

	gst_caps_replace(&plugin->srcpad_caps, NULL);
	plugin->srcpad_caps_changed = FALSE;
	gst_video_info_init(&plugin->srcpad_info);
	gst_caps_replace(&plugin->allowed_raw_caps, NULL);
}

/**
* gst_mfx_plugin_base_has_display_type:
* @plugin: a #GstMfxPluginBase
* @display_type_req: the desired #GstMfxDisplayType
*
* Checks whether the @plugin elements already has a #GstMfxDisplay
* instance compatible with type @display_type_req.
*
* Return value: %TRUE if @plugin has a compatible display, %FALSE otherwise
*/
gboolean
gst_mfx_plugin_base_has_display_type(GstMfxPluginBase * plugin,
	GstMfxDisplayType display_type_req)
{
	GstMfxDisplayType display_type;

	if (!plugin->display)
		return FALSE;

	display_type = plugin->display_type;
	if (gst_mfx_display_type_is_compatible(display_type, display_type_req))
		return TRUE;

	display_type = gst_mfx_display_get_class_type(plugin->display);
	if (gst_mfx_display_type_is_compatible(display_type, display_type_req))
		return TRUE;
	return FALSE;
}

/**
* gst_mfx_plugin_base_set_display_type:
* @plugin: a #GstMfxPluginBase
* @display_type: the new request #GstMfxDisplayType
*
* Requests a new display type. The change is effective at the next
* call to gst_mfx_plugin_base_ensure_display().
*/
void
gst_mfx_plugin_base_set_display_type(GstMfxPluginBase * plugin,
	GstMfxDisplayType display_type)
{
	plugin->display_type_req = display_type;
}

/**
* gst_mfx_plugin_base_set_display_name:
* @plugin: a #GstMfxPluginBase
* @display_name: the new display name to match
*
* Sets the name of the display to look for. The change is effective
* at the next call to gst_mfx_plugin_base_ensure_display().
*/
void
gst_mfx_plugin_base_set_display_name(GstMfxPluginBase * plugin,
	const gchar * display_name)
{
	g_free(plugin->display_name);
	plugin->display_name = g_strdup(display_name);
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
gst_mfx_plugin_base_ensure_display(GstMfxPluginBase * plugin)
{
	if (gst_mfx_plugin_base_has_display_type(plugin, plugin->display_type_req))
		return TRUE;
	gst_mfx_display_replace(&plugin->display, NULL);

	if (!gst_mfx_ensure_display(GST_ELEMENT(plugin),
		plugin->display_type_req))
		return FALSE;
	plugin->display_type = gst_mfx_display_get_display_type(plugin->display);

	GST_MFX_PLUGIN_BASE_GET_CLASS(plugin)->display_changed(plugin);
	return TRUE;
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
ensure_sinkpad_buffer_pool(GstMfxPluginBase * plugin, GstCaps * caps)
{
	GstBufferPool *pool;
	GstCaps *pool_caps;
	GstStructure *config;
	GstVideoInfo vi;
	gboolean need_pool;

	if (!gst_mfx_plugin_base_ensure_display(plugin))
		return FALSE;

	if (plugin->sinkpad_buffer_pool) {
		config = gst_buffer_pool_get_config(plugin->sinkpad_buffer_pool);
		gst_buffer_pool_config_get_params(config, &pool_caps, NULL, NULL, NULL);
		need_pool = !gst_caps_is_equal(caps, pool_caps);
		gst_structure_free(config);
		if (!need_pool)
			return TRUE;
		g_clear_object(&plugin->sinkpad_buffer_pool);
		plugin->sinkpad_buffer_size = 0;
	}

	pool = gst_mfx_video_buffer_pool_new(plugin->display, &plugin->alloc_ctx);
	if (!pool)
		goto error_create_pool;

	gst_video_info_init(&vi);
	gst_video_info_from_caps(&vi, caps);
	if (GST_VIDEO_INFO_FORMAT(&vi) == GST_VIDEO_FORMAT_ENCODED) {
		GST_DEBUG("assume video buffer pool format is NV12");
		gst_video_info_set_format(&vi, GST_VIDEO_FORMAT_NV12,
			GST_VIDEO_INFO_WIDTH(&vi), GST_VIDEO_INFO_HEIGHT(&vi));
	}
	plugin->sinkpad_buffer_size = vi.size;

	config = gst_buffer_pool_get_config(pool);
	gst_buffer_pool_config_set_params(config, caps,
		plugin->sinkpad_buffer_size, 0, 0);
	gst_buffer_pool_config_add_option(config,
		GST_BUFFER_POOL_OPTION_MFX_VIDEO_META);
	gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);
	if (!gst_buffer_pool_set_config(pool, config))
		goto error_pool_config;
	plugin->sinkpad_buffer_pool = pool;
	return TRUE;

	/* ERRORS */
error_create_pool:
	{
		GST_ERROR("failed to create buffer pool");
		return FALSE;
	}
error_pool_config:
	{
		GST_ERROR("failed to reset buffer pool config");
		gst_object_unref(pool);
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

	if (!ensure_sinkpad_buffer_pool(plugin, plugin->sinkpad_caps))
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
gst_mfx_plugin_base_propose_allocation(GstMfxPluginBase * plugin,
GstQuery * query)
{
	GstCaps *caps = NULL;
	gboolean need_pool;

	gst_query_parse_allocation(query, &caps, &need_pool);

	if (need_pool) {
		if (!caps)
			goto error_no_caps;
		if (!ensure_sinkpad_buffer_pool(plugin, caps))
			return FALSE;
		gst_query_add_allocation_pool(query, plugin->sinkpad_buffer_pool,
			plugin->sinkpad_buffer_size, 0, 0);
	}

	gst_query_add_allocation_meta(query, GST_MFX_VIDEO_META_API_TYPE, NULL);
	gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);
	return TRUE;

	/* ERRORS */
error_no_caps:
	{
		GST_INFO_OBJECT(plugin, "no caps specified");
		return FALSE;
	}
error_pool_config:
	{
		GST_ERROR_OBJECT(plugin, "failed to reset buffer pool config");
		return FALSE;
	}
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

	g_return_val_if_fail(plugin->display != NULL, FALSE);

	gst_query_parse_allocation(query, &caps, NULL);

	if (!caps)
		goto error_no_caps;

	has_video_meta = gst_query_find_allocation_meta(query,
		GST_VIDEO_META_API_TYPE, NULL);

	/* Make sure the display we pass down to the buffer pool is actually
	the expected one, especially when the downstream element requires
	a GLX or EGL display */
	if (!gst_mfx_plugin_base_ensure_display(plugin))
		goto error_ensure_display;

	gst_video_info_init(&vi);
	gst_video_info_from_caps(&vi, caps);
	if (GST_VIDEO_INFO_FORMAT(&vi) == GST_VIDEO_FORMAT_ENCODED)
		gst_video_info_set_format(&vi, GST_VIDEO_FORMAT_I420,
		GST_VIDEO_INFO_WIDTH(&vi), GST_VIDEO_INFO_HEIGHT(&vi));

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
			"Pool hasn't GstMfxVideoMeta");
		if (pool)
			gst_object_unref(pool);
		pool = gst_mfx_video_buffer_pool_new(plugin->display, &plugin->alloc_ctx);
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
error_ensure_display:
	{
		GST_ERROR_OBJECT(plugin, "failed to ensure display of type %d",
			plugin->display_type_req);
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
gst_mfx_plugin_base_get_input_buffer(GstMfxPluginBase * plugin,
	GstBuffer * inbuf, GstBuffer ** outbuf_ptr)
{
	GstMfxVideoMeta *meta;
	GstBuffer *outbuf;
	GstVideoFrame src_frame, out_frame;
	gboolean success;

	g_return_val_if_fail(inbuf != NULL, GST_FLOW_ERROR);
	g_return_val_if_fail(outbuf_ptr != NULL, GST_FLOW_ERROR);

	meta = gst_buffer_get_mfx_video_meta(inbuf);
	if (meta) {
		*outbuf_ptr = gst_buffer_ref(inbuf);
		return GST_FLOW_OK;
	}

	if (!plugin->sinkpad_caps_is_raw)
		goto error_invalid_buffer;

	if (!plugin->sinkpad_buffer_pool)
		goto error_no_pool;

	if (!gst_buffer_pool_set_active(plugin->sinkpad_buffer_pool, TRUE))
		goto error_active_pool;

	outbuf = NULL;
	if (gst_buffer_pool_acquire_buffer(plugin->sinkpad_buffer_pool,
		&outbuf, NULL) != GST_FLOW_OK)
		goto error_create_buffer;

	if (!gst_video_frame_map(&src_frame, &plugin->sinkpad_info, inbuf,
		GST_MAP_READ))
		goto error_map_src_buffer;

	if (!gst_video_frame_map(&out_frame, &plugin->sinkpad_info, outbuf,
		GST_MAP_WRITE))
		goto error_map_dst_buffer;

	success = gst_video_frame_copy(&out_frame, &src_frame);
	gst_video_frame_unmap(&out_frame);
	gst_video_frame_unmap(&src_frame);
	if (!success)
		goto error_copy_buffer;

done:
	gst_buffer_copy_into(outbuf, inbuf, GST_BUFFER_COPY_FLAGS |
		GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
	*outbuf_ptr = outbuf;
	return GST_FLOW_OK;

	/* ERRORS */
error_no_pool:
	{
		GST_ELEMENT_ERROR(plugin, STREAM, FAILED,
			("no buffer pool was negotiated"), ("no buffer pool was negotiated"));
		return GST_FLOW_ERROR;
	}
error_active_pool:
	{
		GST_ELEMENT_ERROR(plugin, STREAM, FAILED,
			("failed to activate buffer pool"), ("failed to activate buffer pool"));
		return GST_FLOW_ERROR;
	}
error_map_dst_buffer:
	{
		gst_video_frame_unmap(&src_frame);
		// fall-through
	}
error_map_src_buffer:
	{
		GST_WARNING("failed to map buffer");
		gst_buffer_unref(outbuf);
		return GST_FLOW_NOT_SUPPORTED;
	}

	/* ERRORS */
error_invalid_buffer:
	{
		GST_ELEMENT_ERROR(plugin, STREAM, FAILED,
			("failed to validate source buffer"),
			("failed to validate source buffer"));
		return GST_FLOW_ERROR;
	}
error_create_buffer:
	{
		GST_ELEMENT_ERROR(plugin, STREAM, FAILED, ("Allocation failed"),
			("failed to create buffer"));
		return GST_FLOW_ERROR;
	}
error_copy_buffer:
	{
		GST_WARNING("failed to upload buffer to VA surface");
		gst_buffer_unref(outbuf);
		return GST_FLOW_NOT_SUPPORTED;
	}
}
