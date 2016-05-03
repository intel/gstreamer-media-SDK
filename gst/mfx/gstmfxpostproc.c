#include "gstmfxcompat.h"
#include <gst/video/video.h>

#include "gstmfxpostproc.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideobufferpool.h"
#include "gstmfxvideomemory.h"

#define GST_PLUGIN_NAME "mfxvpp"
#define GST_PLUGIN_DESC "A video postprocessing filter"

GST_DEBUG_CATEGORY_STATIC(gst_debug_mfxpostproc);
#define GST_CAT_DEFAULT gst_debug_mfxpostproc

/* Default templates */
/* *INDENT-OFF* */
static const char gst_mfxpostproc_sink_caps_str[] =
	GST_MFX_MAKE_SURFACE_CAPS ", "
	GST_CAPS_INTERLACED_MODES "; "
	GST_VIDEO_CAPS_MAKE("{ NV12, YV12, UYVY, YUY2, BGRA, BGRx }") ", "
	GST_CAPS_INTERLACED_MODES;
/* *INDENT-ON* */

/* *INDENT-OFF* */
static const char gst_mfxpostproc_src_caps_str[] =
	GST_MFX_MAKE_SURFACE_CAPS ", "
	GST_CAPS_INTERLACED_MODES "; "
	GST_VIDEO_CAPS_MAKE("{ NV12, BGRA }") ", "
	GST_CAPS_INTERLACED_MODES;
/* *INDENT-ON* */

/* *INDENT-OFF* */
static GstStaticPadTemplate gst_mfxpostproc_sink_factory =
	GST_STATIC_PAD_TEMPLATE("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(gst_mfxpostproc_sink_caps_str));
/* *INDENT-ON* */

/* *INDENT-OFF* */
static GstStaticPadTemplate gst_mfxpostproc_src_factory =
	GST_STATIC_PAD_TEMPLATE("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(gst_mfxpostproc_src_caps_str));
/* *INDENT-ON* */


G_DEFINE_TYPE_WITH_CODE(
	GstMfxPostproc,
	gst_mfxpostproc,
	GST_TYPE_BASE_TRANSFORM,
	GST_MFX_PLUGIN_BASE_INIT_INTERFACES);


enum
{
	PROP_0,

	PROP_FORMAT,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_FORCE_ASPECT_RATIO,
	PROP_DEINTERLACE_MODE,
	PROP_DENOISE,
	PROP_DETAIL,
	PROP_HUE,
	PROP_SATURATION,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
    PROP_ROTATION,
    PROP_FRAMERATE_CONVERSION,
};

#define DEFAULT_FORMAT                  GST_VIDEO_FORMAT_NV12
#define DEFAULT_DEINTERLACE_MODE        GST_MFX_DEINTERLACE_MODE_NONE
#define DEFAULT_ROTATION                GST_MFX_ROTATION_0
#define DEFAULT_FRC_ALG                 GST_MFX_FRC_NONE

#define GST_MFX_TYPE_DEINTERLACE_MODE \
	gst_mfx_deinterlace_mode_get_type()
#define GST_MFX_ROTATION_MODE \
    gst_mfx_rotation_get_type()
#define GST_MFX_FRC_ALGORITHM \
    gst_mfx_frc_alg_get_type()

static GType
gst_mfx_deinterlace_mode_get_type(void)
{
	static GType deinterlace_mode_type = 0;

	static const GEnumValue mode_types[] = {
		{ GST_MFX_DEINTERLACE_MODE_NONE,
		"None", "none" },
		{ GST_MFX_DEINTERLACE_MODE_BOB,
		"Bob deinterlacing", "bob" },
		{ GST_MFX_DEINTERLACE_MODE_ADVANCED,
		"Advanced deinterlacing", "adi" },
		{ GST_MFX_DEINTERLACE_MODE_ADVANCED_NOREF,
		"Advanced deinterlacing with no reference", "adi-noref" },
		{ 0, NULL, NULL },
	};

	if (!deinterlace_mode_type) {
		deinterlace_mode_type =
			g_enum_register_static("GstMfxDeinterlaceMode", mode_types);
	}
	return deinterlace_mode_type;
}

static GType
gst_mfx_rotation_get_type(void)
{
    static GType rotation_value = 0;

    static const GEnumValue rotation_modes[] = {
        { GST_MFX_ROTATION_0,
            "No rotation", "0"},
        { GST_MFX_ROTATION_90,
            "Rotate by 90", "clockwise", "90"},
        { GST_MFX_ROTATION_180,
            "Rotate by 180", "clockwise", "180"},
        { GST_MFX_ROTATION_270,
            "Rotate by 270", "clockwise", "270"},
        {0, NULL, NULL},
    };
    if (!rotation_value)
        rotation_value =
            g_enum_register_static("GstMfxRotation", rotation_modes);
    return rotation_value;
}


static GType
gst_mfx_frc_alg_get_type(void)
{
    static GType alg = 0;
    static const GEnumValue frc_alg[] = {
        { GST_MFX_FRC_NONE,
            "No framerate conversion algorithm", "0"},
        { GST_MFX_FRC_PRESERVE_TIMESTAMP,
            "Frame dropping/repetition based FRC with preserved original timestamps.", "frc-preserve-ts"},
        { GST_MFX_FRC_DISTRIBUTED_TIMESTAMP,
            "Frame dropping/repetition based FRC with distributed timestamps.", "frc-distributed-ts"},
        /*{ GST_MFX_FRC_FRAME_INTERPOLATION,
            "Frame interpolation FRC.", "fi"},
        { GST_MFX_FRC_FI_PRESERVE_TIMESTAMP,
            "Frame dropping/repetition and frame interpolation FRC with preserved original timestamps.", "fi-preserve-ts"},
        { GST_MFX_FRC_FI_DISTRIBUTED_TIMESTAMP,
            "Frame dropping/repetition and frame interpolation FRC with distributed timestamps.", "fi-distributed-ts"},*/
        {0, NULL, NULL},
    };
    if (!alg)
        alg = g_enum_register_static("GstMfxFrcAlgorithm", frc_alg);
    return alg;
}

static void
find_best_size(GstMfxPostproc * postproc, GstVideoInfo * vip,
	guint * width_ptr, guint * height_ptr)
{
	guint width, height;

	width = GST_VIDEO_INFO_WIDTH(vip);
	height = GST_VIDEO_INFO_HEIGHT(vip);
	if (postproc->width && postproc->height) {
		width = postproc->width;
		height = postproc->height;
	}
	else if (postproc->keep_aspect) {
		const gdouble ratio = (gdouble)width / height;
		if (postproc->width) {
			width = postproc->width;
			height = postproc->width / ratio;
		}
		else if (postproc->height) {
			height = postproc->height;
			width = postproc->height * ratio;
		}
	}
	else if (postproc->width)
		width = postproc->width;
	else if (postproc->height)
		height = postproc->height;

    if (GST_MFX_ROTATION_90 == postproc->angle ||
            GST_MFX_ROTATION_270 == postproc->angle) {
        width = width ^ height;
        height = width ^ height;
        width = width ^ height;
    }
	*width_ptr = width;
	*height_ptr = height;
}

static inline gboolean
gst_mfxpostproc_ensure_aggregator(GstMfxPostproc * vpp)
{
	return
		gst_mfx_plugin_base_ensure_aggregator(GST_MFX_PLUGIN_BASE(vpp));
}

static gboolean
gst_mfxpostproc_ensure_filter(GstMfxPostproc * vpp)
{
	GstMfxPluginBase *plugin = GST_MFX_PLUGIN_BASE(vpp);
	GstMfxTask *task;
	gboolean mapped;

	if (vpp->filter)
		return TRUE;

	if (!gst_mfxpostproc_ensure_aggregator(plugin))
		return FALSE;

    if (!plugin->mapped) {
        task = gst_mfx_task_aggregator_get_current_task(plugin->aggregator);
        if (task && gst_mfx_task_has_mapped_surface(task))
            plugin->mapped = TRUE;
    }

	gst_caps_replace(&vpp->allowed_srcpad_caps, NULL);
	gst_caps_replace(&vpp->allowed_sinkpad_caps, NULL);

	vpp->filter = gst_mfx_filter_new(plugin->aggregator,
                        plugin->mapped, plugin->mapped);
	if (!vpp->filter)
		return FALSE;
	return TRUE;
}

static gboolean
gst_mfxpostproc_update_src_caps(GstMfxPostproc * vpp, GstCaps * caps,
	gboolean * caps_changed_ptr)
{
	GST_INFO_OBJECT(vpp, "new src caps = %" GST_PTR_FORMAT, caps);

	if (!video_info_update(caps, &vpp->srcpad_info, caps_changed_ptr))
		return FALSE;

	if (GST_VIDEO_INFO_FORMAT(&vpp->sinkpad_info) !=
        GST_VIDEO_INFO_FORMAT(&vpp->srcpad_info))
		vpp->flags |= GST_MFX_POSTPROC_FLAG_FORMAT;

	if ((vpp->width || vpp->height) &&
		vpp->width != GST_VIDEO_INFO_WIDTH(&vpp->sinkpad_info) &&
		vpp->height != GST_VIDEO_INFO_HEIGHT(&vpp->sinkpad_info))
		vpp->flags |= GST_MFX_POSTPROC_FLAG_SIZE;

	return TRUE;
}

static gboolean
video_info_changed(GstVideoInfo * old_vip, GstVideoInfo * new_vip)
{
	if (GST_VIDEO_INFO_FORMAT(old_vip) != GST_VIDEO_INFO_FORMAT(new_vip))
		return TRUE;
	if (GST_VIDEO_INFO_INTERLACE_MODE(old_vip) !=
		GST_VIDEO_INFO_INTERLACE_MODE(new_vip))
		return TRUE;
	if (GST_VIDEO_INFO_WIDTH(old_vip) != GST_VIDEO_INFO_WIDTH(new_vip))
		return TRUE;
	if (GST_VIDEO_INFO_HEIGHT(old_vip) != GST_VIDEO_INFO_HEIGHT(new_vip))
		return TRUE;
	return FALSE;
}

gboolean
video_info_update(GstCaps * caps, GstVideoInfo * info,
	gboolean * caps_changed_ptr)
{
	GstVideoInfo vi;

	if (!gst_video_info_from_caps(&vi, caps))
		return FALSE;

	*caps_changed_ptr = FALSE;
	if (video_info_changed(info, &vi)) {
		*caps_changed_ptr = TRUE;
		*info = vi;
	}

	return TRUE;
}

static gboolean
gst_mfxpostproc_update_sink_caps(GstMfxPostproc * vpp, GstCaps * caps,
	gboolean * caps_changed_ptr)
{
	GstVideoInfo vi;
	gboolean deinterlace;

	GST_INFO_OBJECT(vpp, "new sink caps = %" GST_PTR_FORMAT, caps);

	if (!video_info_update(caps, &vpp->sinkpad_info, caps_changed_ptr))
		return FALSE;

	vi = vpp->sinkpad_info;
	if (GST_VIDEO_INFO_IS_INTERLACED(&vi) &&
		vpp->deinterlace_mode != GST_MFX_DEINTERLACE_MODE_NONE)
		vpp->flags |= GST_MFX_POSTPROC_FLAG_DEINTERLACING;

	vpp->get_va_surfaces = gst_caps_has_mfx_surface(caps);
	return TRUE;
}

static GstBuffer *
create_output_buffer(GstMfxPostproc * postproc)
{
    GstBuffer *outbuf;
    GstFlowReturn ret;

    GstBufferPool *const pool =
        GST_MFX_PLUGIN_BASE (postproc)->srcpad_buffer_pool;

    g_return_val_if_fail(pool != NULL, NULL);

    if(!gst_buffer_pool_set_active( pool, TRUE ))
        goto error_activate_pool;

    outbuf = NULL;

    ret = gst_buffer_pool_acquire_buffer (pool, &outbuf, NULL);

    if ( GST_FLOW_OK != ret || !outbuf)
        goto error_create_buffer;
    return outbuf;

    /* Errors */
error_activate_pool:
    {
        GST_ERROR ("failed to activate output video buffer pool");
        return NULL;
    }
error_create_buffer:
    {
        GST_ERROR ("failed to create output video buffer");
        return NULL;
    }
}

static GstFlowReturn
gst_mfxpostproc_transform(GstBaseTransform * trans, GstBuffer * inbuf,
	GstBuffer * outbuf)
{
	GstMfxPostproc *const vpp = GST_MFXPOSTPROC(trans);
	GstMfxVideoMeta *inbuf_meta, *outbuf_meta;
	GstMfxSurfaceProxy *proxy, *out_proxy;
	GstMfxFilterStatus status;
	guint flags;
	GstFlowReturn ret;
	GstMfxRectangle *crop_rect = NULL;
	GstMfxRectangle tmp_rect;
    GstBuffer *buf;
    GstClockTime timestamp;

	timestamp = GST_BUFFER_TIMESTAMP(inbuf);

	ret =
      gst_mfx_plugin_base_get_input_buffer (GST_MFX_PLUGIN_BASE (vpp),
      inbuf, &buf);
    if (ret != GST_FLOW_OK)
        return GST_FLOW_ERROR;

    inbuf_meta = gst_buffer_get_mfx_video_meta(buf);
    proxy = gst_mfx_video_meta_get_surface_proxy(inbuf_meta);
    if (!proxy)
        goto error_create_proxy;

    do {
        if (vpp->flags & GST_MFX_POSTPROC_FLAG_FRC) {
            buf = create_output_buffer(vpp);
            if (!buf)
                goto error_create_buffer;
        }

        status = gst_mfx_filter_process(vpp->filter, proxy, &out_proxy);
        if ( GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == status )
            outbuf_meta = gst_buffer_get_mfx_video_meta(buf);
        else
            outbuf_meta = gst_buffer_get_mfx_video_meta(outbuf);

        if (!outbuf_meta)
            goto error_create_meta;

        gst_mfx_video_meta_set_surface_proxy(outbuf_meta, out_proxy);
        crop_rect = gst_mfx_surface_proxy_get_crop_rect(out_proxy);
        if(crop_rect) {
             GstVideoCropMeta *const crop_meta =
                 gst_buffer_add_video_crop_meta(outbuf);
             if(crop_meta) {
                 crop_meta->x = crop_rect->x;
                 crop_meta->y = crop_rect->y;
                 crop_meta->width = crop_rect->width;
                 crop_meta->height = crop_rect->height;
            }
        }

        if( GST_MFX_FILTER_STATUS_ERROR_MORE_DATA == status )
            return GST_BASE_TRANSFORM_FLOW_DROPPED;

        if( GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == status ) {
            GST_BUFFER_TIMESTAMP(buf) = timestamp;
            GST_BUFFER_DURATION(buf) = vpp->field_duration;
            timestamp += vpp->field_duration;
            ret = gst_pad_push(trans->srcpad, buf);
        } else {
            if (vpp->flags & GST_MFX_POSTPROC_FLAG_FRC) {
                GST_BUFFER_TIMESTAMP(outbuf) = timestamp;
                GST_BUFFER_DURATION(outbuf) = vpp->field_duration;
            } else
                gst_buffer_copy_into(outbuf, inbuf, GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
        }

        if( GST_MFX_FILTER_STATUS_SUCCESS != status &&
                GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE != status)
            goto error_process_vpp;

    } while (GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE == status);

	return GST_FLOW_OK;

	/* ERRORS */
error_create_buffer:
    {
        GST_ERROR("failed to output buffer");
        return GST_FLOW_ERROR;
    }
error_invalid_buffer:
	{
		GST_ERROR("failed to validate source buffer");
		return GST_FLOW_ERROR;
	}
error_create_meta:
	{
		GST_ERROR("failed to create new output buffer meta");
		return GST_FLOW_ERROR;
	}
error_create_proxy:
	{
		GST_ERROR("failed to create surface proxy from buffer");
		return GST_FLOW_ERROR;
	}
error_process_vpp:
	{
		GST_ERROR("failed to apply VPP (error %d)", status);
		return GST_FLOW_ERROR;
	}
}

static gboolean
gst_mfxpostproc_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
    GstMfxPostproc *const vpp = GST_MFXPOSTPROC (trans);
    GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (trans);

    if (vpp->get_va_surfaces)
        return FALSE;

    if (!gst_mfx_plugin_base_propose_allocation (plugin, query))
        return FALSE;
    return TRUE;
}


static gboolean
gst_mfxpostproc_decide_allocation(GstBaseTransform * trans, GstQuery * query)
{
	return gst_mfx_plugin_base_decide_allocation(GST_MFX_PLUGIN_BASE(trans),
		query);
}

static gboolean
ensure_allowed_sinkpad_caps(GstMfxPostproc * vpp)
{
	GstCaps *out_caps, *raw_caps;

	if (vpp->allowed_sinkpad_caps)
		return TRUE;

    out_caps = gst_static_pad_template_get_caps(&gst_mfxpostproc_sink_factory);

	if (!out_caps) {
		GST_ERROR_OBJECT(vpp, "failed to create MFX sink caps");
		return FALSE;
	}

	vpp->allowed_sinkpad_caps = out_caps;

	return TRUE;
}

static gboolean
ensure_allowed_srcpad_caps(GstMfxPostproc * vpp)
{
	GstCaps *out_caps;

	if (vpp->allowed_srcpad_caps)
		return TRUE;

	/* Create initial caps from pad template */
	out_caps = gst_caps_from_string(gst_mfxpostproc_src_caps_str);
	if (!out_caps) {
		GST_ERROR("failed to create MFX src caps");
		return FALSE;
	}

	vpp->allowed_srcpad_caps = out_caps;

	return TRUE;
}

static GstCaps *
gst_mfxpostproc_transform_caps_impl(GstBaseTransform * trans,
	GstPadDirection direction, GstCaps * caps)
{
	GstMfxPostproc *const vpp = GST_MFXPOSTPROC(trans);
	GstVideoInfo vi, peer_vi;
	GstVideoFormat out_format;
	GstCaps *out_caps, *peer_caps;
	GstMfxCapsFeature feature;
	const gchar *feature_str;
    guint width, height, fps_n, fps_d;

	/* Generate the sink pad caps, that could be fixated afterwards */
	if (direction == GST_PAD_SRC) {
		if (!ensure_allowed_sinkpad_caps(vpp))
			return NULL;
		return gst_caps_ref(vpp->allowed_sinkpad_caps);
	}

	/* Generate complete set of src pad caps if non-fixated sink pad
	caps are provided */
	if (!gst_caps_is_fixed(caps)) {
		if (!ensure_allowed_srcpad_caps(vpp))
			return NULL;
		return gst_caps_ref(vpp->allowed_srcpad_caps);
	}

	/* Generate the expected src pad caps, from the current fixated
	sink pad caps */
	if (!gst_video_info_from_caps(&vi, caps))
		return NULL;

    if (vpp->deinterlace_mode)
        GST_VIDEO_INFO_INTERLACE_MODE(&vi) = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

	/* Update size from user-specified parameters */
	find_best_size(vpp, &vi, &width, &height);

	/* Update format from user-specified parameters */
    peer_caps = gst_pad_peer_query_caps(
        GST_BASE_TRANSFORM_SRC_PAD(trans),
        vpp->allowed_srcpad_caps
        );

    if(gst_caps_is_any(peer_caps) || gst_caps_is_empty(peer_caps))
        return peer_caps;
    if(!gst_caps_is_fixed(peer_caps))
        peer_caps = gst_caps_fixate(peer_caps);

    gst_video_info_from_caps(&peer_vi, peer_caps);
    out_format = GST_VIDEO_INFO_FPS_N(&peer_vi);
    fps_n = GST_VIDEO_INFO_FPS_N(&peer_vi);
    fps_d = GST_VIDEO_INFO_FPS_D(&peer_vi);

    if (vpp->format != DEFAULT_FORMAT)
        out_format = vpp->format;

    if(fps_n != GST_VIDEO_INFO_FPS_N(&vpp->sinkpad_info) && 0 != fps_n) {
         vpp->fps_n = fps_n;
         vpp->fps_d = fps_d;
         GST_VIDEO_INFO_FPS_N(&vi) = fps_n;
         GST_VIDEO_INFO_FPS_D(&vi) = fps_d;
         vpp->field_duration = gst_util_uint64_scale(
                 GST_SECOND,
                 vpp->fps_d,
                 vpp->fps_n
                 );
         vpp->flags |= GST_MFX_POSTPROC_FLAG_FRC;
         if ( DEFAULT_FRC_ALG == vpp->alg )
             vpp->alg = GST_MFX_FRC_PRESERVE_TIMESTAMP;
    }

    if(peer_caps)
        gst_caps_unref(peer_caps);

	feature =
		gst_mfx_find_preferred_caps_feature(GST_BASE_TRANSFORM_SRC_PAD(trans),
		&out_format);
	gst_video_info_change_format(&vi, out_format, width, height);

	out_caps = gst_video_info_to_caps(&vi);
	if (!out_caps)
		return NULL;


	if (feature) {
		feature_str = gst_mfx_caps_feature_to_string(feature);
		if (feature_str)
			gst_caps_set_features(out_caps, 0,
                gst_caps_features_new(feature_str, NULL));
	}

	if (vpp->format != out_format)
		vpp->format = out_format;

	return out_caps;
}

static GstCaps *
gst_mfxpostproc_transform_caps(GstBaseTransform * trans,
	GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
	GstCaps *out_caps;

	caps = gst_mfxpostproc_transform_caps_impl(trans, direction, caps);
	if (caps && filter) {
		out_caps = gst_caps_intersect_full(caps, filter, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(caps);

		return out_caps;
	}

	return caps;
}

static gboolean
gst_mfxpostproc_transform_size(GstBaseTransform * trans,
	GstPadDirection direction, GstCaps * caps, gsize size,
	GstCaps * othercaps, gsize * othersize)
{
	GstMfxPostproc *const vpp = GST_MFXPOSTPROC(trans);

	if (direction == GST_PAD_SINK || vpp->get_va_surfaces)
		*othersize = 0;
	else
		*othersize = size;
	return TRUE;
}

static gboolean
gst_mfxpostproc_create(GstMfxPostproc * vpp)
{
	if (!gst_mfxpostproc_ensure_filter(vpp))
        return FALSE;

	gst_mfx_filter_set_frame_info(vpp->filter, &vpp->sinkpad_info);

	if (!gst_mfx_filter_set_size(vpp->filter, GST_VIDEO_INFO_WIDTH(&vpp->srcpad_info),
        GST_VIDEO_INFO_HEIGHT(&vpp->srcpad_info)))
		return FALSE;

    if ((vpp->flags & GST_MFX_POSTPROC_FLAG_FORMAT) &&
		!gst_mfx_filter_set_format(vpp->filter, vpp->format))
		return FALSE;

    if ((vpp->flags & GST_MFX_POSTPROC_FLAG_DENOISE) &&
		!gst_mfx_filter_set_denoising_level(vpp->filter,
		vpp->denoise_level))
		return FALSE;

	if ((vpp->flags & GST_MFX_POSTPROC_FLAG_DETAIL) &&
		!gst_mfx_filter_set_detail_level(vpp->filter,
		vpp->detail_level))
		return FALSE;

	if ((vpp->flags & GST_MFX_POSTPROC_FLAG_HUE) &&
		!gst_mfx_filter_set_hue(vpp->filter, vpp->hue))
		return FALSE;

	if ((vpp->flags & GST_MFX_POSTPROC_FLAG_SATURATION) &&
		!gst_mfx_filter_set_saturation(vpp->filter, vpp->saturation))
		return FALSE;

	if ((vpp->flags & GST_MFX_POSTPROC_FLAG_BRIGHTNESS) &&
		!gst_mfx_filter_set_brightness(vpp->filter, vpp->brightness))
		return FALSE;

	if ((vpp->flags & GST_MFX_POSTPROC_FLAG_CONTRAST) &&
		!gst_mfx_filter_set_contrast(vpp->filter, vpp->contrast))
		return FALSE;

    if((vpp->flags & GST_MFX_POSTPROC_FLAG_ROTATION) &&
        !gst_mfx_filter_set_rotation(vpp->filter, vpp->angle))
        return FALSE;

	if ((vpp->flags & GST_MFX_POSTPROC_FLAG_DEINTERLACING) &&
        !gst_mfx_filter_set_deinterlace_mode(vpp->filter, vpp->deinterlace_mode))
		return FALSE;

    if ((vpp->flags & GST_MFX_POSTPROC_FLAG_FRC) &&
            !(gst_mfx_filter_set_frc_algorithm(vpp->filter, vpp->alg) &&
            gst_mfx_filter_set_framerate(vpp->filter, vpp->fps_n, vpp->fps_d)))
        return FALSE;

	return gst_mfx_filter_start(vpp->filter);
}

static void
gst_mfxpostproc_destroy(GstMfxPostproc * vpp)
{
	gst_mfx_filter_replace(&vpp->filter, NULL);
	gst_caps_replace(&vpp->allowed_sinkpad_caps, NULL);
	gst_caps_replace(&vpp->allowed_srcpad_caps, NULL);
	gst_mfx_plugin_base_close(GST_MFX_PLUGIN_BASE(vpp));
}

static gboolean
gst_mfxpostproc_set_caps(GstBaseTransform * trans, GstCaps * caps,
	GstCaps * out_caps)
{
	GstMfxPostproc *const vpp = GST_MFXPOSTPROC(trans);
	gboolean caps_changed = FALSE;
	GstVideoInfo vinfo;

	if (!gst_mfxpostproc_update_sink_caps(vpp, caps, &caps_changed))
		return FALSE;

	if (!gst_mfxpostproc_update_src_caps(vpp, out_caps, &caps_changed))
		return FALSE;

	if (caps_changed) {
		gst_mfxpostproc_destroy(vpp);

		if (!gst_mfx_plugin_base_set_caps(GST_MFX_PLUGIN_BASE(vpp),
			caps, out_caps))
			return FALSE;
		if (!gst_mfxpostproc_create(vpp))
			return FALSE;
	}

	return TRUE;
}

static gboolean
gst_mfxpostproc_query(GstBaseTransform * trans, GstPadDirection direction,
	GstQuery * query)
{
	GstMfxPostproc *const vpp = GST_MFXPOSTPROC(trans);

	if (GST_QUERY_TYPE(query) == GST_QUERY_CONTEXT) {
		if (gst_mfx_handle_context_query(query,
			GST_MFX_PLUGIN_BASE_AGGREGATOR(vpp))) {
			GST_DEBUG_OBJECT(vpp, "sharing tasks %p",
				GST_MFX_PLUGIN_BASE_AGGREGATOR(vpp));
			return TRUE;
		}
	}

	return
		GST_BASE_TRANSFORM_CLASS(gst_mfxpostproc_parent_class)->query(trans,
		direction, query);
}

static void
gst_mfxpostproc_finalize(GObject * object)
{
	GstMfxPostproc *const postproc = GST_MFXPOSTPROC(object);

	gst_mfxpostproc_destroy(postproc);

	gst_mfx_plugin_base_finalize(GST_MFX_PLUGIN_BASE(postproc));
	G_OBJECT_CLASS(gst_mfxpostproc_parent_class)->finalize(object);
}


static void
gst_mfxpostproc_set_property(GObject * object,
	guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstMfxPostproc *const vpp = GST_MFXPOSTPROC(object);

	switch (prop_id) {
	case PROP_FORMAT:
		vpp->format = g_value_get_enum(value);
		break;
	case PROP_WIDTH:
		vpp->width = g_value_get_uint(value);
		break;
	case PROP_HEIGHT:
		vpp->height = g_value_get_uint(value);
		break;
	case PROP_FORCE_ASPECT_RATIO:
		vpp->keep_aspect = g_value_get_boolean(value);
		break;
	case PROP_DEINTERLACE_MODE:
		vpp->deinterlace_mode = g_value_get_enum(value);
		break;
	case PROP_DENOISE:
		vpp->denoise_level = g_value_get_uint(value);
		vpp->flags |= GST_MFX_POSTPROC_FLAG_DENOISE;
		break;
	case PROP_DETAIL:
		vpp->detail_level = g_value_get_uint(value);
		vpp->flags |= GST_MFX_POSTPROC_FLAG_DETAIL;
		break;
	case PROP_HUE:
		vpp->hue = g_value_get_float(value);
		vpp->flags |= GST_MFX_POSTPROC_FLAG_HUE;
		break;
	case PROP_SATURATION:
		vpp->saturation = g_value_get_float(value);
		vpp->flags |= GST_MFX_POSTPROC_FLAG_SATURATION;
		break;
	case PROP_BRIGHTNESS:
		vpp->brightness = g_value_get_float(value);
		vpp->flags |= GST_MFX_POSTPROC_FLAG_BRIGHTNESS;
		break;
	case PROP_CONTRAST:
		vpp->contrast = g_value_get_float(value);
		vpp->flags |= GST_MFX_POSTPROC_FLAG_CONTRAST;
		break;
    case PROP_ROTATION:
        vpp->angle = g_value_get_enum(value);
        vpp->flags |= GST_MFX_POSTPROC_FLAG_ROTATION;
        break;
    case PROP_FRAMERATE_CONVERSION:
        vpp->alg = g_value_get_enum(value);
        vpp->flags |= GST_MFX_POSTPROC_FLAG_FRC;
        break;
    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}


static void
gst_mfxpostproc_get_property(GObject * object,
	guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstMfxPostproc *const postproc = GST_MFXPOSTPROC(object);

	switch (prop_id) {
	case PROP_FORMAT:
		g_value_set_enum(value, postproc->format);
		break;
	case PROP_WIDTH:
		g_value_set_uint(value, postproc->width);
		break;
	case PROP_HEIGHT:
		g_value_set_uint(value, postproc->height);
		break;
	case PROP_FORCE_ASPECT_RATIO:
		g_value_set_boolean(value, postproc->keep_aspect);
		break;
	case PROP_DEINTERLACE_MODE:
		g_value_set_enum(value, postproc->deinterlace_mode);
		break;
	case PROP_DENOISE:
		g_value_set_uint(value, postproc->denoise_level);
		break;
	case PROP_DETAIL:
		g_value_set_uint(value, postproc->detail_level);
		break;
	case PROP_HUE:
		g_value_set_float(value, postproc->hue);
		break;
	case PROP_SATURATION:
		g_value_set_float(value, postproc->saturation);
		break;
	case PROP_BRIGHTNESS:
		g_value_set_float(value, postproc->brightness);
		break;
	case PROP_CONTRAST:
		g_value_set_float(value, postproc->contrast);
		break;
    case PROP_ROTATION:
        g_value_set_enum(value, postproc->angle);
        break;
    case PROP_FRAMERATE_CONVERSION:
        g_value_set_enum(value, postproc->alg);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}


static void
gst_mfxpostproc_class_init(GstMfxPostprocClass * klass)
{
	GObjectClass *const object_class = G_OBJECT_CLASS(klass);
	GstElementClass *const element_class = GST_ELEMENT_CLASS(klass);
	GstBaseTransformClass *const trans_class = GST_BASE_TRANSFORM_CLASS(klass);
	GstPadTemplate *pad_template;

	GST_DEBUG_CATEGORY_INIT(gst_debug_mfxpostproc,
		GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

	gst_mfx_plugin_base_class_init(GST_MFX_PLUGIN_BASE_CLASS(klass));

	object_class->finalize = gst_mfxpostproc_finalize;
	object_class->set_property = gst_mfxpostproc_set_property;
	object_class->get_property = gst_mfxpostproc_get_property;
	trans_class->transform_caps = gst_mfxpostproc_transform_caps;
	trans_class->transform_size = gst_mfxpostproc_transform_size;
	trans_class->transform = gst_mfxpostproc_transform;
	trans_class->set_caps = gst_mfxpostproc_set_caps;
	trans_class->query = gst_mfxpostproc_query;
	trans_class->propose_allocation = gst_mfxpostproc_propose_allocation;
	trans_class->decide_allocation = gst_mfxpostproc_decide_allocation;

	gst_element_class_set_static_metadata(element_class,
		"MFX video postprocessing",
		"Filter/Converter/Video;Filter/Converter/Video/Scaler;"
		"Filter/Effect/Video;Filter/Effect/Video/Deinterlace",
		GST_PLUGIN_DESC, "Ishmael Sameen <ishmael.visayana.sameen@intel.com>");

	/* sink pad */
	pad_template = gst_static_pad_template_get(&gst_mfxpostproc_sink_factory);
	gst_element_class_add_pad_template(element_class, pad_template);

	/* src pad */
	pad_template = gst_static_pad_template_get(&gst_mfxpostproc_src_factory);
	gst_element_class_add_pad_template(element_class, pad_template);

	/**
	* GstMfxVpp:deinterlace-mode:
	*
	* This selects whether the deinterlacing should always be applied
	* or if they should only be applied on content that has the
	* "interlaced" flag on the caps.
	*/
	g_object_class_install_property
		(object_class,
		PROP_DEINTERLACE_MODE,
		g_param_spec_enum("deinterlace-mode",
		"Deinterlace mode",
		"Deinterlace mode to use",
		GST_MFX_TYPE_DEINTERLACE_MODE,
		DEFAULT_DEINTERLACE_MODE,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:width:
	*
	* The forced output width in pixels. If set to zero, the width is
	* calculated from the height if aspect ration is preserved, or
	* inherited from the sink caps width
	*/
	g_object_class_install_property
		(object_class,
		PROP_WIDTH,
		g_param_spec_uint("width",
		"Width",
		"Forced output width",
		0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:height:
	*
	* The forced output height in pixels. If set to zero, the height is
	* calculated from the width if aspect ration is preserved, or
	* inherited from the sink caps height
	*/
	g_object_class_install_property
		(object_class,
		PROP_HEIGHT,
		g_param_spec_uint("height",
		"Height",
		"Forced output height",
		0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:force-aspect-ratio:
	*
	* When enabled, scaling respects video aspect ratio; when disabled,
	* the video is distorted to fit the width and height properties.
	*/
	g_object_class_install_property
		(object_class,
		PROP_FORCE_ASPECT_RATIO,
		g_param_spec_boolean("force-aspect-ratio",
		"Force aspect ratio",
		"When enabled, scaling will respect original aspect ratio",
		TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:denoise:
	*
	* The level of noise reduction to apply.
	*/
	g_object_class_install_property(object_class,
		PROP_DENOISE,
		g_param_spec_uint("denoise",
		"Denoising Level",
		"The level of denoising to apply",
		0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:detail:
	*
	* The level of detail / edge enhancement to apply for positive values.
	*/
	g_object_class_install_property(object_class,
		PROP_DETAIL,
		g_param_spec_uint("detail",
		"Detail Level",
		"The level of detail / edge enhancement to apply",
		0, 100, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:hue:
	*
	* The color hue, expressed as a float value. Range is -180.0 to
	* 180.0. Default value is 0.0 and represents no modification.
	*/
	g_object_class_install_property(object_class,
		PROP_HUE,
		g_param_spec_float("hue",
		"Hue",
		"The color hue value",
		-180.0, 180.0, 0.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:saturation:
	*
	* The color saturation, expressed as a float value. Range is 0.0 to
	* 10.0. Default value is 1.0 and represents no modification.
	*/
	g_object_class_install_property(object_class,
		PROP_SATURATION,
		g_param_spec_float("saturation",
		"Saturation",
		"The color saturation value",
		0.0, 10.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:brightness:
	*
	* The color brightness, expressed as a float value. Range is -100.0
	* to 100.0. Default value is 0.0 and represents no modification.
	*/
	g_object_class_install_property(object_class,
		PROP_BRIGHTNESS,
		g_param_spec_float("brightness",
		"Brightness",
		"The color brightness value",
		-100.0, 100.0, 0.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxPostproc:contrast:
	*
	* The color contrast, expressed as a float value. Range is 0.0 to
	* 10.0. Default value is 1.0 and represents no modification.
	*/
	g_object_class_install_property(object_class,
		PROP_CONTRAST,
		g_param_spec_float("contrast",
		"Contrast",
		"The color contrast value",
		0.0, 10.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * GstMfxPostproc:rotation:
    *
    * The rotation angle  for the surface, expressed in GstMfxRotation.
    */
    g_object_class_install_property(object_class,
            PROP_ROTATION,
            g_param_spec_enum("rotation",
                "Rotation",
                "The rotation angle",
                GST_MFX_ROTATION_MODE,
                DEFAULT_ROTATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * GstMfxPostproc: framerate conversion:
     * The framerate conversion algorithm to convert framerate of the video,
     * expressed in GstMfxFrcAlgorithm.
     */
    g_object_class_install_property(object_class,
            PROP_FRAMERATE_CONVERSION,
            g_param_spec_enum("frc-algorithm",
                "Algorithm",
                "The algorithm type",
                GST_MFX_FRC_ALGORITHM,
                DEFAULT_FRC_ALG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mfxpostproc_init(GstMfxPostproc * vpp)
{
	gst_mfx_plugin_base_init(GST_MFX_PLUGIN_BASE(vpp),
		GST_CAT_DEFAULT);

	vpp->format = DEFAULT_FORMAT;
	vpp->deinterlace_mode = DEFAULT_DEINTERLACE_MODE;
	vpp->keep_aspect = TRUE;
    vpp->alg = DEFAULT_FRC_ALG;

	gst_video_info_init(&vpp->sinkpad_info);
	gst_video_info_init(&vpp->srcpad_info);
}
