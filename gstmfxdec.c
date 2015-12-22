/*
 ============================================================================
 Name        : gst-mfx-dec.c
 Author      : Ishmael Sameen <ishmael.visayana.sameen@intel.com>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2015
 Description :
 ============================================================================
 */

#include "gstmfxdec.h"

#include <string.h>

#include "gstvaapiimage.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurfaceproxy_priv.h"
#include "gstmfxcodecmap.h"
#include "gstmfxvideomemory.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideobufferpool.h"
#include "gstmfxpluginutil.h"

GST_DEBUG_CATEGORY_STATIC(mfxdec_debug);
#define GST_CAT_DEFAULT (mfxdec_debug)

#define DEFAULT_ASYNC_DEPTH 4

/* Default templates */
#define GST_CAPS_CODEC(CODEC) CODEC "; "

/* *INDENT-OFF* */
static const char gst_mfxdecode_sink_caps_str[] =
	GST_CAPS_CODEC("video/mpeg, mpegversion=2, systemstream=(boolean)false")
	GST_CAPS_CODEC("video/x-h264, stream-format = (string) { byte-stream }, alignment = (string) { au, nal }")
	GST_CAPS_CODEC("video/x-wmv")
	;

static const char gst_mfxdecode_src_caps_str[] =
	GST_MFX_MAKE_SURFACE_CAPS ";"
	//GST_MFX_MAKE_GLTEXUPLOAD_CAPS ";"
	GST_VIDEO_CAPS_MAKE("{ NV12, I420, YV12 }");

enum
{
	PROP_0,
	PROP_ASYNC_DEPTH
};

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(gst_mfxdecode_sink_caps_str)
);

static GstStaticPadTemplate src_template_factory =
	GST_STATIC_PAD_TEMPLATE("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	//GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("NV12"))
	GST_STATIC_CAPS(gst_mfxdecode_src_caps_str)
);

#define gst_mfxdec_parent_class parent_class
G_DEFINE_TYPE(GstMfxDec, gst_mfxdec, GST_TYPE_VIDEO_DECODER);

static void gst_mfx_dec_set_property(GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec);
static void gst_mfx_dec_get_property(GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec);

/* GstVideoDecoder base class method */
static gboolean gst_mfxdec_open(GstVideoDecoder * decoder);
static gboolean gst_mfxdec_close(GstVideoDecoder * decoder);
static gboolean gst_mfxdec_start(GstVideoDecoder * decoder);
static gboolean gst_mfxdec_stop(GstVideoDecoder * decoder);
static gboolean gst_mfxdec_set_format(GstVideoDecoder * decoder,
	GstVideoCodecState * state);
static gboolean gst_mfxdec_flush(GstVideoDecoder * decoder);
static GstFlowReturn gst_mfxdec_handle_frame(GstVideoDecoder * decoder,
	GstVideoCodecFrame * frame);
static gboolean gst_mfxdec_decide_allocation(GstVideoDecoder * decoder,
	GstQuery * query);


static void
gst_mfxdec_class_init (GstMfxDecClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS(klass);

	gobject_class->set_property = gst_mfx_dec_set_property;
	gobject_class->get_property = gst_mfx_dec_get_property;

	g_object_class_install_property(gobject_class, PROP_ASYNC_DEPTH,
		g_param_spec_uint("async-depth", "Asynchronous Depth",
		"Number of async operations before explicit sync",
		0, 16, DEFAULT_ASYNC_DEPTH,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get(&src_template_factory));
	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get(&sink_template_factory));
	gst_element_class_set_static_metadata(element_class,
		"MFX Video Decoder",
		"Codec/Decoder/Video",
		"Uses libmfx for decoding video streams",
		"Ishmael Sameen<ishmael.visayana.sameen@intel.com>");

	video_decoder_class->open = GST_DEBUG_FUNCPTR(gst_mfxdec_open);
	video_decoder_class->close = GST_DEBUG_FUNCPTR(gst_mfxdec_close);
	video_decoder_class->start = GST_DEBUG_FUNCPTR(gst_mfxdec_start);
	video_decoder_class->stop = GST_DEBUG_FUNCPTR(gst_mfxdec_stop);
	video_decoder_class->flush = GST_DEBUG_FUNCPTR(gst_mfxdec_flush);
	video_decoder_class->set_format = GST_DEBUG_FUNCPTR(gst_mfxdec_set_format);
	video_decoder_class->handle_frame =
		GST_DEBUG_FUNCPTR(gst_mfxdec_handle_frame);
	video_decoder_class->decide_allocation =
		GST_DEBUG_FUNCPTR(gst_mfxdec_decide_allocation);

	GST_DEBUG_CATEGORY_INIT(mfxdec_debug, "mfxdec", 0, "MFX Video Decoder");
}

static void
gst_mfxdec_init (GstMfxDec *mfxdec)
{
	mfxdec->async_depth = DEFAULT_ASYNC_DEPTH;

	gst_video_decoder_set_packetized(GST_VIDEO_DECODER(mfxdec), TRUE);
	gst_video_decoder_set_needs_format(GST_VIDEO_DECODER(mfxdec), TRUE);
}

static void
gst_mfx_dec_set_property(GObject * object, guint prop_id,
const GValue * value, GParamSpec * pspec)
{
	GstMfxDec *dec;

	g_return_if_fail(GST_IS_MFXDEC(object));
	dec = GST_MFXDEC(object);

	GST_DEBUG_OBJECT(object, "gst_mfx_dec_set_property");
	switch (prop_id) {
	case PROP_ASYNC_DEPTH:
		dec->async_depth = g_value_get_uint(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gst_mfx_dec_get_property(GObject * object, guint prop_id, GValue * value,
	GParamSpec * pspec)
{
	GstMfxDec *dec;

	g_return_if_fail(GST_IS_MFXDEC(object));
	dec = GST_MFXDEC(object);

	switch (prop_id) {
	case PROP_ASYNC_DEPTH:
		g_value_set_uint(value, dec->async_depth);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean
gst_mfxdec_open(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

    int va_ver_major, va_ver_minor;
    int err;

	GST_DEBUG_OBJECT(mfxdec, "open");

	memset(&(mfxdec->alloc_ctx), 0, sizeof (GstMfxContextAllocatorVaapi));

    /* initialize VA-API */
    mfxdec->dpy = XOpenDisplay(NULL);
    if (!mfxdec->dpy) {
        GST_ERROR ("Cannot open the X display\n");
        return FALSE;
    }

    mfxdec->alloc_ctx.va_dpy = vaGetDisplay(mfxdec->dpy);
    if (!mfxdec->alloc_ctx.va_dpy) {
        GST_ERROR ("Cannot open the VA display\n");
        return FALSE;
    }

    err = vaInitialize(mfxdec->alloc_ctx.va_dpy, &va_ver_major, &va_ver_minor);
    if (err != VA_STATUS_SUCCESS) {
        GST_ERROR ("Cannot initialize VA: %s\n", vaErrorStr(err));
        return FALSE;
    }

    GST_INFO_OBJECT("Initialized VA v%d.%d\n", va_ver_major, va_ver_minor);

	return TRUE;
}

static gboolean
gst_mfxdec_close(GstVideoDecoder * decoder)
{
	GstMfxDec *const decode = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(decode, "close");

	return TRUE;
}

static gboolean
gst_mfxdec_start(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(mfxdec, "start");

	//if (!gst_mfx_decoder_ensure_context(decoder))
		//return FALSE;

	return TRUE;
}

static gboolean
gst_mfxdec_stop(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(mfxdec, "stop");

	if (mfxdec->output_state) {
		gst_video_codec_state_unref(mfxdec->output_state);
		mfxdec->output_state = NULL;
	}

	if (mfxdec->input_state) {
		gst_video_codec_state_unref(mfxdec->input_state);
		mfxdec->input_state = NULL;
	}

	return TRUE;
}

static gboolean
gst_mfxdec_flush(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(mfxdec, "stop");

	if (mfxdec->output_state) {
		gst_video_codec_state_unref(mfxdec->output_state);
		mfxdec->output_state = NULL;
	}

	return TRUE;
}

static gboolean
gst_mfxdec_create(GstMfxDec * mfxdec, GstCaps * caps)
{
	mfxU32 codec_id = gst_get_mfx_codec_from_caps(caps);

	if (codec_id < 0)
		return FALSE;

	mfxdec->decoder = gst_mfx_decoder_new(&mfxdec->alloc_ctx, codec_id);
	if (!mfxdec->decoder)
		return FALSE;

	return TRUE;
}

static gboolean
gst_mfxdec_set_format(GstVideoDecoder * decoder, GstVideoCodecState * state)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(mfxdec, "set_format");

	if (mfxdec->output_state) {
		gst_video_codec_state_unref(mfxdec->output_state);
		mfxdec->output_state = NULL;
	}

	/* Save input state to be used as reference for output state */
	if (mfxdec->input_state) {
		gst_video_codec_state_unref(mfxdec->input_state);
		mfxdec->input_state = NULL;
	}

	mfxdec->input_state = gst_video_codec_state_ref(state);

	return gst_mfxdec_create(mfxdec, state->caps);
}

static gboolean
gst_mfxdec_update_src_caps(GstMfxDec * decode)
{
	GstVideoDecoder *const vdec = GST_VIDEO_DECODER(decode);
	GstVideoCodecState *state, *ref_state;
	GstVideoInfo *vi;
	GstVideoFormat format = GST_VIDEO_FORMAT_I420;

	if (!decode->input_state)
		return FALSE;

	ref_state = decode->input_state;

	GstCapsFeatures *features = NULL;
	GstMfxCapsFeature feature;

	feature =
		gst_mfx_find_preferred_caps_feature(GST_VIDEO_DECODER_SRC_PAD(vdec),
		GST_VIDEO_INFO_FORMAT(&ref_state->info), &format);

	if (feature == GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED)
		return FALSE;

	switch (feature) {
	case GST_MFX_CAPS_FEATURE_GL_TEXTURE_UPLOAD_META:
		features =
			gst_caps_features_new
			(GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META, NULL);
		break;

	case GST_MFX_CAPS_FEATURE_MFX_SURFACE:
		features =
			gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_MFX_SURFACE, NULL);
		break;

	default:
		break;
	}

	state = gst_video_decoder_set_output_state(vdec, format,
		ref_state->info.width, ref_state->info.height, ref_state);
	if (!state || state->info.width == 0 || state->info.height == 0)
		return FALSE;

	vi = &state->info;

	state->caps = gst_video_info_to_caps(vi);
	if (features)
		gst_caps_set_features(state->caps, 0, features);
	GST_INFO_OBJECT(decode, "new src caps = %" GST_PTR_FORMAT, state->caps);

	decode->output_state = state;

	return TRUE;
}

static GstFlowReturn
gst_mfxdec_push_decoded_frame(GstMfxDec *decode, GstVideoCodecFrame * frame)
{
	GstFlowReturn ret;
	GstMfxDecoderStatus sts;
	GstMfxSurfaceProxy *proxy;
	GstMfxVideoMeta *meta;
	const GstMfxRectangle *crop_rect;

	sts = gst_mfx_decoder_get_surface_proxy(decode->decoder, &proxy);

	ret = gst_video_decoder_allocate_output_frame(GST_VIDEO_DECODER(decode), frame);

	if (ret != GST_FLOW_OK)
		goto error_create_buffer;

	meta = gst_buffer_get_mfx_video_meta(frame->output_buffer);
	gst_mfx_video_meta_set_surface_proxy(meta, proxy);
	crop_rect = gst_mfx_surface_proxy_get_crop_rect(proxy);
	if (crop_rect) {
		GstVideoCropMeta *const crop_meta =
			gst_buffer_add_video_crop_meta(frame->output_buffer);
		if (crop_meta) {
			crop_meta->x = crop_rect->x;
			crop_meta->y = crop_rect->y;
			crop_meta->width = crop_rect->width;
			crop_meta->height = crop_rect->height;
		}
	}

	//if (decode->has_texture_upload_meta)
		//gst_buffer_ensure_texture_upload_meta(frame->output_buffer);

	ret = gst_video_decoder_finish_frame(GST_VIDEO_DECODER(decode), frame);
	if (ret != GST_FLOW_OK)
		goto error_commit_buffer;

	return ret;

	/* ERRORS */
error_create_buffer:
	{
		gst_video_decoder_drop_frame(GST_VIDEO_DECODER(decode), frame);
		gst_video_codec_frame_unref(frame);
		return GST_FLOW_ERROR;
	}
error_get_meta:
	{
		gst_video_decoder_drop_frame(GST_VIDEO_DECODER(decode), frame);
		gst_video_codec_frame_unref(frame);
		return GST_FLOW_ERROR;
	}
error_commit_buffer:
	{
		GST_INFO_OBJECT(decode, "downstream element rejected the frame (%s [%d])",
			gst_flow_get_name(ret), ret);
		gst_video_codec_frame_unref(frame);
		return GST_FLOW_ERROR;
	}
}

static GstFlowReturn
gst_mfxdec_handle_frame(GstVideoDecoder *vdec, GstVideoCodecFrame * frame)
{
	GstMfxDec *mfxdec = GST_MFXDEC(vdec);
	GstMfxDecoderStatus sts;
	GstFlowReturn ret = GST_FLOW_OK;

	if (!mfxdec->input_state)
		goto not_negotiated;

	sts = gst_mfx_decoder_decode(mfxdec->decoder, frame);

	switch (sts) {
	case GST_MFX_DECODER_STATUS_READY:
		if (!gst_mfxdec_update_src_caps(mfxdec))
			goto not_negotiated;
		if (!gst_video_decoder_negotiate(vdec))
			goto not_negotiated;
		/* Fall through */
	case GST_MFX_DECODER_STATUS_ERROR_NO_DATA:
	case GST_MFX_DECODER_STATUS_ERROR_NO_SURFACE:
		GST_VIDEO_CODEC_FRAME_FLAG_SET(frame,
			GST_VIDEO_CODEC_FRAME_FLAG_DECODE_ONLY);
		ret = GST_FLOW_OK;
		break;
	case GST_MFX_DECODER_STATUS_SUCCESS:
		ret = gst_mfxdec_push_decoded_frame(mfxdec, frame);
		break;
	case GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED:
	case GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER:
		goto error_decode;
	default:
		ret = GST_FLOW_ERROR;
	}

	return ret;

error_decode:
	{
		GST_ERROR("MFX decode error %d", sts);
		gst_video_decoder_drop_frame(vdec, frame);
		return GST_FLOW_NOT_SUPPORTED;
	}
not_negotiated:
	{
		GST_ERROR_OBJECT(mfxdec, "not negotiated");
		gst_video_decoder_drop_frame(vdec, frame);
		return GST_FLOW_NOT_SUPPORTED;
	}
}

/* XXXX: GStreamer 1.2 doesn't check, in gst_buffer_pool_set_config()
if the config option is already set */
static inline gboolean
gst_mfxdec_set_pool_config(GstBufferPool * pool,
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

gboolean
gst_mfxdec_decide_allocation(GstVideoDecoder * vdec, GstQuery * query)
{
	GstMfxDec *const decode = GST_MFXDEC(vdec);
	GstCaps *caps = NULL;
	GstBufferPool *pool;
	GstStructure *config;
	GstVideoInfo vi;
	guint size, min, max;
	gboolean update_pool = FALSE;
	gboolean has_video_meta = FALSE;
	gboolean has_video_alignment = FALSE;

	gboolean has_texture_upload_meta = FALSE;
	guint idx;

	gst_query_parse_allocation(query, &caps, NULL);

	/* We don't need any GL context beyond this point if not requested
	so explicitly through GstVideoGLTextureUploadMeta */
	//gst_object_replace(&plugin->gl_context, NULL);

	if (!caps)
		goto error_no_caps;

	has_video_meta = gst_query_find_allocation_meta(query,
		GST_VIDEO_META_API_TYPE, NULL);

	has_texture_upload_meta = gst_query_find_allocation_meta(query,
		GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, &idx);

	/*if (has_texture_upload_meta) {
        const GstStructure *params;
        GstObject *gl_context;

        gst_query_parse_nth_allocation_meta(query, idx, &params);
        if (params) {
            if (gst_structure_get(params, "gst.gl.GstGLContext", GST_GL_TYPE_CONTEXT,
                    &gl_context, NULL) && gl_context) {
                gst_mfxdec_set_gl_context(decode, gl_context);
                gst_object_unref(gl_context);
            }
        }
	}*/

	gst_video_info_init(&vi);
	gst_video_info_from_caps(&vi, caps);
	if (GST_VIDEO_INFO_FORMAT(&vi) == GST_VIDEO_FORMAT_ENCODED)
		gst_video_info_set_format(&vi, GST_VIDEO_FORMAT_NV12,
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

	/* GstVaapiVideoMeta is mandatory, and this implies VA surface memory */
    if (!pool || !gst_buffer_pool_has_option (pool,
            GST_BUFFER_POOL_OPTION_MFX_VIDEO_META)) {
        GST_INFO_OBJECT (decode, "%s. Making a new pool", pool == NULL ? "No pool" :
            "Pool hasn't GstMfxVideoMeta");
        if (pool)
            gst_object_unref (pool);
        pool = gst_mfx_video_buffer_pool_new (&decode->alloc_ctx);
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
		if (!gst_mfxdec_set_pool_config(pool,
			GST_BUFFER_POOL_OPTION_VIDEO_META))
			goto config_failed;
	}
	else if (has_video_alignment) {
		if (!gst_mfxdec_set_pool_config(pool,
			GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT))
			goto config_failed;
	}

	/* GstVideoGLTextureUploadMeta (OpenGL) */
	if (has_texture_upload_meta) {
		if (!gst_mfxdec_set_pool_config(pool,
			GST_BUFFER_POOL_OPTION_VIDEO_GL_TEXTURE_UPLOAD_META))
			goto config_failed;
	}

	if (update_pool)
		gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
	else
		gst_query_add_allocation_pool(query, pool, size, min, max);

	//g_clear_object (&decode->srcpad_buffer_pool);
    //decode->srcpad_buffer_pool = pool;
    return TRUE;

error_no_caps:
	{
		GST_ERROR_OBJECT(vdec, "no caps specified");
		return FALSE;
	}
error_create_pool:
    {
        GST_ERROR_OBJECT (vdec, "failed to create buffer pool");
        return FALSE;
    }
config_failed:
	{
		if (pool)
			gst_object_unref(pool);
        GST_ELEMENT_ERROR(vdec, RESOURCE, SETTINGS,
            ("Failed to configure the buffer pool"),
			("Configuration is most likely invalid, please report this issue."));
		return FALSE;
	}
}
