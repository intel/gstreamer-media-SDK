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

#include "gstvaapialloc.h"

#include <string.h>

#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

// MSDK Helper macro definitions
#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_SAFE_DELETE_ARRAY(P)       {if (P) {free(P); P = NULL;}}


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
	GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("NV12"))
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
gst_mfxdec_init_session(GstMfxDec *mfxdec)
{
	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { { 1, 1 } };

	const char *desc;
	int ret;

	ret = MFXInit(impl, &ver, &(mfxdec->session));
	if (ret < 0) {
		GST_ERROR_OBJECT(mfxdec, "Error initializing an internal MFX session");
		return FALSE;
	}

	MFXQueryIMPL(mfxdec->session, &impl);

	switch (MFX_IMPL_BASETYPE(impl)) {
	case MFX_IMPL_SOFTWARE:
		desc = "software";
		break;
	case MFX_IMPL_HARDWARE:
	case MFX_IMPL_HARDWARE2:
	case MFX_IMPL_HARDWARE3:
	case MFX_IMPL_HARDWARE4:
		desc = "hardware accelerated";
		break;
	default:
		desc = "unknown";
	}

	GST_LOG_OBJECT(mfxdec, "Initialized an internal MFX session using %s implementation",
		desc);

	return TRUE;
}

static gboolean
gst_mfxdec_open(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

    int va_ver_major, va_ver_minor;
    int err;

	GST_DEBUG_OBJECT(mfxdec, "open");

    /* initialize VA-API */
    mfxdec->dpy = XOpenDisplay(NULL);
    if (!mfxdec->dpy) {
        g_printerr("Cannot open the X display\n");
        return FALSE;
    }

    mfxdec->decode.va_dpy = vaGetDisplay(mfxdec->dpy);
    if (!mfxdec->decode.va_dpy) {
        g_printerr("Cannot open the VA display\n");
        return FALSE;
    }

    err = vaInitialize(mfxdec->decode.va_dpy, &va_ver_major, &va_ver_minor);
    if (err != VA_STATUS_SUCCESS) {
        g_printerr("Cannot initialize VA: %s\n", vaErrorStr(err));
        return FALSE;
    }
    g_print("Initialized VA v%d.%d\n", va_ver_major, va_ver_minor);

	if (!gst_mfxdec_init_session(mfxdec))
		return FALSE;

    MFXVideoCORE_SetHandle(mfxdec->session, MFX_HANDLE_VA_DISPLAY, mfxdec->decode.va_dpy);

	return TRUE;
}

static gboolean
gst_mfxdec_close(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(mfxdec, "close");

	MFXClose(mfxdec->session);
    if (mfxdec->decode.va_dpy)
        vaTerminate(mfxdec->decode.va_dpy);
    if (mfxdec->dpy)
        XCloseDisplay(mfxdec->dpy);

	return TRUE;
}

static gboolean
gst_mfxdec_start(GstVideoDecoder * decoder)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	mfxFrameAllocator frame_allocator = {
        .pthis = &mfxdec->decode,
        .Alloc = frame_alloc,
        .Lock  = frame_lock,
        .Unlock = frame_unlock,
        .GetHDL = frame_get_hdl,
        .Free   = frame_free,
    };

	GST_DEBUG_OBJECT(mfxdec, "start");

	/* make sure the decoder is uninitialized */
	MFXVideoDECODE_Close(mfxdec->session);
	mfxdec->decoder_inited = FALSE;

	MFXVideoCORE_SetFrameAllocator(mfxdec->session, &frame_allocator);

	return TRUE;
}

static void
release_resources(GstMfxDec *mfxdec)
{
	MFXVideoDECODE_Close(mfxdec->session);

	MSDK_SAFE_DELETE_ARRAY(mfxdec->bs.Data);
	MSDK_SAFE_DELETE_ARRAY(mfxdec->surface_buffers);
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

	GST_DEBUG_OBJECT(mfxdec, "flush");

	if (mfxdec->output_state) {
		gst_video_codec_state_unref(mfxdec->output_state);
		mfxdec->output_state = NULL;
	}

	if (mfxdec->decoder_inited)
		release_resources(mfxdec);
	mfxdec->decoder_inited = FALSE;

	return TRUE;
}

static void
gst_mfx_params_with_caps(GstMfxDec *mfxdec, GstVideoCodecState * state)
{
	GstStructure *structure = gst_caps_get_structure(state->caps, 0);
	const gchar *mimetype;

	mimetype = gst_structure_get_name(structure);

	if (!strcmp(mimetype, "video/x-h264")) {
		mfxdec->param.mfx.CodecId = MFX_CODEC_AVC;
	}
	else if (!strcmp(mimetype, "video/x-wmv")) {
		mfxdec->param.mfx.CodecId = MFX_CODEC_VC1;
	}
	else if (!strcmp(mimetype, "video/mpeg")) {
		gint mpegversion;

		gst_structure_get_int(structure, "mpegversion", &mpegversion);
		if (mpegversion == 2)
			mfxdec->param.mfx.CodecId = MFX_CODEC_MPEG2;
	}
}

static gboolean
gst_mfxdec_set_format(GstVideoDecoder * decoder, GstVideoCodecState * state)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);

	GST_DEBUG_OBJECT(mfxdec, "set_format");

	if (mfxdec->decoder_inited)
		release_resources(mfxdec);
	mfxdec->decoder_inited = FALSE;

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

	memset(&mfxdec->param, 0, sizeof (mfxVideoParam));

	gst_mfx_params_with_caps(mfxdec, state);

	mfxdec->param.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

	return TRUE;
}

static void release_buffer(QSVFrame *frame)
{
    *frame->surface_index = 0;
    g_free(frame->surface);
    frame->surface = NULL;
}

static gboolean get_buffer(GstMfxDec *mfxdec, QSVFrame *frame)
{
    DecodeContext *decode = &mfxdec->decode;
    int idx;

    for (idx = 0; idx < decode->nb_surfaces; idx++) {
        if (!decode->surface_used[idx])
            break;
    }

    if (idx == decode->nb_surfaces) {
        g_printerr("No free surfaces\n");
        return FALSE;
    }

    decode->surface_used[idx] = 1;
    frame->surface->Info       = decode->frame_info;
    frame->surface->Data.MemId = &decode->surfaces[idx];
    frame->surface_index = &decode->surface_used[idx];

    return TRUE;
}

void qsv_clear_unused_frames(GstMfxDec *mfxdec)
{
    QSVFrame *cur = mfxdec->work_frames;
    while (cur) {
        if (cur->surface && !cur->surface->Data.Locked) {
            //cur->surface = NULL;
            release_buffer(cur);
        }
        cur = cur->next;
    }
}

static gboolean get_surface(GstMfxDec *mfxdec, mfxFrameSurface1 **surf)
{
    QSVFrame *frame, **last;

    qsv_clear_unused_frames(mfxdec);

    frame = mfxdec->work_frames;
    last  = &mfxdec->work_frames;
    while (frame) {
        if (!frame->surface) {
            frame->surface = g_malloc0(sizeof(*frame->surface));
            if (!frame->surface)
                return FALSE;
            if(!get_buffer(mfxdec, frame))
                return FALSE;
            *surf = frame->surface;
            return TRUE;
        }

        last  = &frame->next;
        frame = frame->next;
    }

    frame = g_malloc0(sizeof(*frame));
    if (!frame)
        return FALSE;
    *last = frame;

    frame->surface = g_malloc0(sizeof(*frame->surface));
    if (!frame->surface)
        return FALSE;
    if(!get_buffer(mfxdec, frame))
        return FALSE;

    *surf = frame->surface;

    return TRUE;
}

static void
gst_mfxdec_vasurface_to_buffer(GstMfxDec * dec, const mfxFrameSurface1 * surf,
GstBuffer * buffer)
{
	int deststride, srcstride, height, width, line, plane;
	guint8 *dest, *src;
	GstVideoFrame frame;
	GstVideoInfo *info = &dec->output_state->info;

	VASurfaceID    surface = *(VASurfaceID*)surf->Data.MemId;
    VAImageFormat img_fmt = {
        .fourcc         = VA_FOURCC_NV12,
        .byte_order     = VA_LSB_FIRST,
        .bits_per_pixel = 8,
        .depth          = 8,
    };

    VAImage img;

    VAStatus err;

    int va_width = MSDK_ALIGN32(surf->Info.Width);
    int va_height = MSDK_ALIGN32(surf->Info.Height);

    DecodeContext *decode = &dec->decode;

	if (!gst_video_frame_map(&frame, info, buffer, GST_MAP_WRITE)) {
		GST_ERROR_OBJECT(dec, "Could not map video buffer");
		return;
	}

    img.buf      = VA_INVALID_ID;
    img.image_id = VA_INVALID_ID;

    err = vaCreateImage(decode->va_dpy, &img_fmt,
                        va_width, va_height, &img);
    if (err != VA_STATUS_SUCCESS) {
        g_printerr("Error creating an image: %s\n",
                vaErrorStr(err));
        goto fail;
    }

    err = vaGetImage(decode->va_dpy, surface, 0, 0,
                    va_width, va_height,
                    img.image_id);
    if (err != VA_STATUS_SUCCESS) {
        g_printerr("Error getting an image: %s\n",
                vaErrorStr(err));
        goto fail;
    }

    err = vaMapBuffer(decode->va_dpy, img.buf, (void**)&src);
    if (err != VA_STATUS_SUCCESS) {
        g_printerr("Error mapping the image buffer: %s\n",
                vaErrorStr(err));
        goto fail;
    }

	for (plane = 0; plane < img.num_planes; plane++) {
		dest = GST_VIDEO_FRAME_COMP_DATA(&frame, plane);

		width = GST_VIDEO_FRAME_COMP_WIDTH(&frame, plane)
			* GST_VIDEO_FRAME_COMP_PSTRIDE(&frame, plane);
		height = GST_VIDEO_FRAME_COMP_HEIGHT(&frame, plane);
		deststride = GST_VIDEO_FRAME_COMP_STRIDE(&frame, plane);
		srcstride = img.pitches[plane];

		if (srcstride == deststride) {
			GST_TRACE_OBJECT(dec, "Stride matches. Comp %d: %d, copying full plane",
				plane, srcstride);
			memcpy(dest, src + img.offsets[plane], srcstride * height);
		}
		else {
			GST_TRACE_OBJECT(dec, "Stride mismatch. Comp %d: %d != %d, copying "
				"line by line.", plane, srcstride, deststride);
			for (line = 0; line < height; line++) {
				memcpy(dest, src + img.offsets[plane] + line * srcstride, width);
				dest += deststride;
			}
		}
	}

	gst_video_frame_unmap(&frame);

fail:
	if (img.buf != VA_INVALID_ID)
        vaUnmapBuffer(decode->va_dpy, img.buf);
    if (img.image_id != VA_INVALID_ID)
        vaDestroyImage(decode->va_dpy, img.image_id);
}

static GstFlowReturn
init_decoder(GstMfxDec *mfxdec)
{
	GstVideoCodecState *state = mfxdec->input_state;

	mfxStatus sts = MFX_ERR_NONE;

	sts = MFXVideoDECODE_DecodeHeader(mfxdec->session, &mfxdec->bs, &mfxdec->param);
	if (sts < 0) {
		GST_ERROR_OBJECT(mfxdec, "Error parsing sequence header");
		return GST_FLOW_CUSTOM_SUCCESS_1;
	}

	sts = MFXVideoDECODE_Init(mfxdec->session, &mfxdec->param);
	if (sts < 0) {
		GST_ERROR_OBJECT(mfxdec, "Error initializing the MFX video decoder");
		return GST_FLOW_ERROR;
	}

	//mfxdec->param.ExtParam = mfxdec->ext_buffers;
	//mfxdec->param.NumExtParam = mfxdec->nb_ext_buffers;

	g_assert(mfxdec->output_state == NULL);
	mfxdec->output_state =
		gst_video_decoder_set_output_state(GST_VIDEO_DECODER(mfxdec),
			GST_VIDEO_FORMAT_NV12, mfxdec->param.mfx.FrameInfo.CropW,
			mfxdec->param.mfx.FrameInfo.CropH, state);
	gst_video_decoder_negotiate(GST_VIDEO_DECODER(mfxdec));

	mfxdec->decoder_inited = TRUE;

	return GST_FLOW_OK;
}

static GstFlowReturn
gst_mfxdec_handle_frame(GstVideoDecoder *decoder, GstVideoCodecFrame * frame)
{
	GstMfxDec *mfxdec = GST_MFXDEC(decoder);
	GstMapInfo minfo;
	GstFlowReturn ret = GST_FLOW_OK;

	mfxFrameSurface1 *insurf;
	mfxFrameSurface1 *outsurf;
	mfxSyncPoint sync;
	mfxStatus sts = MFX_ERR_NONE;

	if (!gst_buffer_map(frame->input_buffer, &minfo, GST_MAP_READ)) {
		GST_ERROR_OBJECT(mfxdec, "Failed to map input buffer");
		return GST_FLOW_ERROR;
	}

	if (mfxdec->bs.Data == NULL) {
		mfxdec->bs.Data = g_malloc(1024 * 1024);
		mfxdec->bs.MaxLength = 1024 * 1024;
	}

	if (minfo.size) {
		memcpy(mfxdec->bs.Data + mfxdec->bs.DataOffset + mfxdec->bs.DataLength, minfo.data, minfo.size);
		mfxdec->bs.DataLength += minfo.size;
		mfxdec->bs.TimeStamp = GST_BUFFER_PTS(frame->input_buffer);
	}

	/* Initialize the MFX decoder session */
	if (!mfxdec->decoder_inited) {
		GST_VIDEO_CODEC_FRAME_FLAG_SET(frame,
			GST_VIDEO_CODEC_FRAME_FLAG_DECODE_ONLY);

		ret = init_decoder(mfxdec);
		if (GST_FLOW_OK != ret)
			return ret;
	}



	do {
        if (!get_surface(mfxdec, &insurf))
            return GST_FLOW_ERROR;

		sts = MFXVideoDECODE_DecodeFrameAsync(mfxdec->session, &mfxdec->bs,
			insurf, &outsurf, &sync);
		if (sts == MFX_WRN_DEVICE_BUSY)
			g_usleep(1);
	} while (sts == MFX_WRN_DEVICE_BUSY || sts == MFX_ERR_MORE_SURFACE);

	if (sts == MFX_ERR_MORE_DATA || sts > 0) {
		GST_VIDEO_CODEC_FRAME_FLAG_SET(frame,
			GST_VIDEO_CODEC_FRAME_FLAG_DECODE_ONLY);
		return GST_FLOW_OK;
	}

	if (sts != MFX_ERR_NONE &&
		sts != MFX_ERR_MORE_DATA &&
		sts != MFX_WRN_VIDEO_PARAM_CHANGED &&
		sts != MFX_ERR_MORE_SURFACE) {
		GST_ERROR_OBJECT(mfxdec, "Error during MFX decoding.");
		return GST_FLOW_ERROR;
	}

	if (sync) {
		MFXVideoCORE_SyncOperation(mfxdec->session, sync, 60000);

		memmove(mfxdec->bs.Data, mfxdec->bs.Data + mfxdec->bs.DataOffset, mfxdec->bs.DataLength);
		mfxdec->bs.DataOffset = 0;
	}

	gst_buffer_unmap(frame->input_buffer, &minfo);

	ret = gst_video_decoder_allocate_output_frame(decoder, frame);
	if (GST_FLOW_OK == ret) {
		gst_mfxdec_vasurface_to_buffer(mfxdec, outsurf, frame->output_buffer);
		ret = gst_video_decoder_finish_frame(decoder, frame);
	}

	return ret;
}


static gboolean
gst_mfxdec_decide_allocation(GstVideoDecoder * bdec, GstQuery * query)
{
	GstBufferPool *pool;
	GstStructure *config;

	if (!GST_VIDEO_DECODER_CLASS(parent_class)->decide_allocation(bdec, query))
		return FALSE;

	g_assert(gst_query_get_n_allocation_pools(query) > 0);
	gst_query_parse_nth_allocation_pool(query, 0, &pool, NULL, NULL, NULL);
	g_assert(pool != NULL);

	config = gst_buffer_pool_get_config(pool);
	if (gst_query_find_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL)) {
		gst_buffer_pool_config_add_option(config,
			GST_BUFFER_POOL_OPTION_VIDEO_META);
	}
	gst_buffer_pool_set_config(pool, config);
	gst_object_unref(pool);

	return TRUE;
}
