#include <mfxplugin.h>
#include "gstmfxencoder.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxfilter.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxvideometa.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxtask.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define DEFAULT_ENCODER_PRESET GST_MFX_ENCODER_PRESET_MEDIUM

/* Helper function to create a new encoder property object */
static GstMfxEncoderPropData *
prop_new(gint id, GParamSpec * pspec)
{
	GstMfxEncoderPropData *prop;

	if (!id || !pspec)
		return NULL;

	prop = g_slice_new(GstMfxEncoderPropData);
	if (!prop)
		return NULL;

	prop->prop = id;
	prop->pspec = g_param_spec_ref_sink(pspec);
	return prop;
}

/* Helper function to release a property object and any memory held herein */
static void
prop_free(GstMfxEncoderPropData * prop)
{
	if (!prop)
		return;

	if (prop->pspec) {
		g_param_spec_unref(prop->pspec);
		prop->pspec = NULL;
	}
	g_slice_free(GstMfxEncoderPropData, prop);
}

/* Helper function to lookup the supplied property specification */
static GParamSpec *
prop_find_pspec(GstMfxEncoder * encoder, gint prop_id)
{
	GPtrArray *const props = encoder->properties;
	guint i;

	if (props) {
		for (i = 0; i < props->len; i++) {
			GstMfxEncoderPropInfo *const prop = g_ptr_array_index(props, i);
			if (prop->prop == prop_id)
				return prop->pspec;
		}
	}
	return NULL;
}

/* Create a new array of properties, or NULL on error */
GPtrArray *
gst_mfx_encoder_properties_append(GPtrArray * props, gint prop_id,
	GParamSpec * pspec)
{
	GstMfxEncoderPropData *prop;

	if (!props) {
		props = g_ptr_array_new_with_free_func((GDestroyNotify)prop_free);
		if (!props)
			return NULL;
	}

	prop = prop_new(prop_id, pspec);
	if (!prop)
		goto error_allocation_failed;
	g_ptr_array_add(props, prop);
	return props;

	/* ERRORS */
error_allocation_failed:
	{
		GST_ERROR("failed to allocate encoder property info structure");
		g_ptr_array_unref(props);
		return NULL;
	}
}

/* Generate the common set of encoder properties */
GPtrArray *
gst_mfx_encoder_properties_get_default(const GstMfxEncoderClass * klass)
{
	const GstMfxEncoderClassData *const cdata = klass->class_data;
	GPtrArray *props = NULL;

	g_assert(cdata->rate_control_get_type != NULL);

	/**
	* GstMfxEncoder:rate-control:
	*
	* The desired rate control mode, expressed as a #GstMfxRateControl.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_RATECONTROL,
		g_param_spec_enum("rate-control",
		"Rate Control", "Rate control mode",
		cdata->rate_control_get_type(), cdata->default_rate_control,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:bitrate:
	*
	* The desired bitrate, expressed in kbps.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_BITRATE,
		g_param_spec_uint("bitrate",
		"Bitrate (kbps)",
		"The desired bitrate expressed in kbps (0: auto-calculate)",
		0, 100 * 1024, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:idr-interval:
	*
	* IDR-frame interval in terms of I-frames.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_IDR_INTERVAL,
		g_param_spec_uint("idr-interval",
		"IDR Interval",
		"Distance (in I-frames) between IDR frames", 0, G_MAXINT,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:gop-size:
	*
	* Number of pictures within the current GOP
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_GOP_SIZE,
		g_param_spec_uint("gop-size",
		"GOP Size",
		"Number of pictures within the current GOP", 0, G_MAXINT,
		256, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:gop-dist:
	*
	* Distance between I- or P- key frames
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_GOP_REFDIST,
		g_param_spec_uint("gop-distance",
		"GOP Reference Distance",
		"Distance between I- or P- key frames", 0, G_MAXINT,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:num-refs:
	*
	* Number of reference frames
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_NUM_REFS,
		g_param_spec_uint("num-refs",
		"Number of reference frames",
		"Number of reference frames", 0, G_MAXINT,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:num-slices:
	*
	* Number of slices in each video frame
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_NUM_SLICES,
		g_param_spec_uint("num-slices",
		"Number of slices",
		"Number of slices in each video frame", 0, G_MAXINT,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:qpi:
	*
	* Quantization parameter for I-frames
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_QPI,
		g_param_spec_uint("qpi",
		"Quantization parameter for I-frames",
		"Quantization parameter for I-frames", 0, 51,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:qpp:
	*
	* Quantization parameter for P-frames
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_QPP,
		g_param_spec_uint("qpp",
		"Quantization parameter for P-frames",
		"Quantization parameter for P-frames", 0, 51,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:qpb:
	*
	* Quantization parameter for B-frames
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_QPB,
		g_param_spec_uint("qpb",
		"Quantization parameter for B-frames",
		"Quantization parameter for B-frames", 0, 51,
		0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:async-depth:
	*
	* Number of parallel operations before explicit sync
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_ASYNC_DEPTH,
		g_param_spec_uint("async-depth",
		"Asynchronous depth",
		"Number of parallel operations before explicit sync", 0, 16,
		4, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoder:preset:
	*
	* The desired encoder preset option.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_PROP_PRESET,
		g_param_spec_enum("preset",
		"Encoder Preset",
		"Encoder preset option",
		GST_MFX_TYPE_ENCODER_PRESET, DEFAULT_ENCODER_PRESET,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	return props;
}

static gboolean
set_encoding_params(GstMfxEncoder * encoder)
{
	encoder->params.AsyncDepth = 4;
	encoder->params.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
	encoder->params.mfx.RateControlMethod = encoder->rc_method;

	if (encoder->bitrate)
		encoder->params.mfx.TargetKbps = encoder->bitrate;
	if (encoder->gop_refdist)
		encoder->params.mfx.GopRefDist = encoder->gop_refdist;
	if (encoder->gop_size)
		encoder->params.mfx.GopPicSize = encoder->gop_size;
	if (encoder->num_refs)
		encoder->params.mfx.NumRefFrame = encoder->num_refs;
	if (encoder->idr_interval)
		encoder->params.mfx.IdrInterval = encoder->idr_interval;
	if (encoder->num_slices)
		encoder->params.mfx.NumSlice = encoder->num_slices;

	if (encoder->rc_method == GST_MFX_RATECONTROL_CQP) {
		if (encoder->qpi)
			encoder->params.mfx.QPI = encoder->qpi;
		if (encoder->qpp)
			encoder->params.mfx.QPP = encoder->qpp;
		if (encoder->qpb)
			encoder->params.mfx.QPB = encoder->qpb;
	}

	//encoder->params.ExtParam = encoder->extparam_internal;
	//encoder->params.NumExtParam = encoder->nb_extparam_internal;

	return TRUE;
}

static void
gst_mfx_encoder_set_input_params(GstMfxEncoder * encoder)
{
	encoder->params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	encoder->params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	encoder->params.mfx.FrameInfo.PicStruct = GST_VIDEO_INFO_IS_INTERLACED(&encoder->info) ?
		(GST_VIDEO_INFO_FLAG_IS_SET(&encoder->info, GST_VIDEO_FRAME_FLAG_TFF) ?
			MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF)
		: MFX_PICSTRUCT_PROGRESSIVE;

	encoder->params.mfx.FrameInfo.CropX = 0;
	encoder->params.mfx.FrameInfo.CropY = 0;
	encoder->params.mfx.FrameInfo.CropW = encoder->info.width;
	encoder->params.mfx.FrameInfo.CropH = encoder->info.height;
	encoder->params.mfx.FrameInfo.FrameRateExtN =
		encoder->info.fps_n ? encoder->info.fps_n : 30;
	encoder->params.mfx.FrameInfo.FrameRateExtD = encoder->info.fps_d;
	encoder->params.mfx.FrameInfo.AspectRatioW = encoder->info.par_n;
	encoder->params.mfx.FrameInfo.AspectRatioH = encoder->info.par_d;
	encoder->params.mfx.FrameInfo.BitDepthChroma = 8;
	encoder->params.mfx.FrameInfo.BitDepthLuma = 8;

    encoder->params.mfx.FrameInfo.Width = GST_ROUND_UP_16(encoder->info.width);
    encoder->params.mfx.FrameInfo.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == encoder->params.mfx.FrameInfo.PicStruct) ?
        GST_ROUND_UP_16(encoder->info.height) :
        GST_ROUND_UP_32(encoder->info.height);

	encoder->bs.MaxLength = encoder->params.mfx.FrameInfo.Width *
		encoder->params.mfx.FrameInfo.Height * 4;
	encoder->bitstream = g_byte_array_sized_new(encoder->bs.MaxLength);
	if (!encoder->bitstream)
		return FALSE;

	set_encoding_params(encoder);
}

static gboolean
gst_mfx_encoder_init_properties(GstMfxEncoder * encoder,
	GstMfxTaskAggregator * aggregator, GstVideoInfo * info, gboolean mapped)
{
	mfxStatus sts = MFX_ERR_NONE;

	encoder->aggregator = gst_mfx_task_aggregator_ref(aggregator);
	encoder->encode_task = gst_mfx_task_new(encoder->aggregator,
		GST_MFX_TASK_ENCODER);
	if (!encoder->encode_task)
		return FALSE;

	gst_mfx_task_aggregator_set_current_task(encoder->aggregator,
		encoder->encode_task);
	encoder->session = gst_mfx_task_get_session(encoder->encode_task);

	encoder->params.mfx.CodecId = encoder->codec;
	encoder->mapped = mapped;
	encoder->params.IOPattern =
        mapped ? MFX_IOPATTERN_IN_SYSTEM_MEMORY : MFX_IOPATTERN_IN_VIDEO_MEMORY;
	encoder->info = *info;

    if (!mapped)
        gst_mfx_task_use_video_memory(encoder->encode_task);

	return TRUE;
}

/* Base encoder initialization (internal) */
static gboolean
gst_mfx_encoder_init(GstMfxEncoder * encoder,
	GstMfxTaskAggregator * aggregator, GstVideoInfo * info, gboolean mapped)
{
	GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS(encoder);

	g_return_val_if_fail(aggregator != NULL, FALSE);

#define CHECK_VTABLE_HOOK(FUNC) do {            \
	if (!klass->FUNC)                           \
	goto error_invalid_vtable;                \
	} while (0)

	CHECK_VTABLE_HOOK(init);
	CHECK_VTABLE_HOOK(finalize);
	CHECK_VTABLE_HOOK(get_default_properties);

#undef CHECK_VTABLE_HOOK

	if (!klass->init(encoder))
		return FALSE;
	if (!gst_mfx_encoder_init_properties(encoder, aggregator, info, mapped))
		return FALSE;
	return TRUE;

	/* ERRORS */
error_invalid_vtable:
	{
		GST_ERROR("invalid subclass hook (internal error)");
		return FALSE;
	}
}

/* Base encoder cleanup (internal) */
void
gst_mfx_encoder_finalize(GstMfxEncoder * encoder)
{
	GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS(encoder);

	klass->finalize(encoder);

	MFXVideoENCODE_Close(encoder->session);

	g_byte_array_unref(encoder->bitstream);
	gst_mfx_task_aggregator_unref(encoder->aggregator);
	gst_mfx_task_replace(&encoder->encode_task, NULL);
	gst_mfx_surface_pool_unref(encoder->pool);

	if (encoder->properties) {
		g_ptr_array_unref(encoder->properties);
		encoder->properties = NULL;
	}
}

GstMfxEncoder *
gst_mfx_encoder_new(const GstMfxEncoderClass * klass,
	GstMfxTaskAggregator * aggregator, GstVideoInfo * info, gboolean mapped)
{
	GstMfxEncoder *encoder;

	g_return_val_if_fail(aggregator != NULL, NULL);

	encoder = gst_mfx_mini_object_new0(GST_MFX_MINI_OBJECT_CLASS(klass));
	if (!encoder)
		goto error;

	if (!gst_mfx_encoder_init(encoder, aggregator, info, mapped))
		goto error;

	return encoder;
error:
	gst_mfx_mini_object_unref(encoder);
	return NULL;
}

GstMfxEncoder *
gst_mfx_encoder_ref(GstMfxEncoder * encoder)
{
	g_return_val_if_fail(encoder != NULL, NULL);

	return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(encoder));
}

void
gst_mfx_encoder_unref(GstMfxEncoder * encoder)
{
	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(encoder));
}

void
gst_mfx_encoder_replace(GstMfxEncoder ** old_encoder_ptr,
	GstMfxEncoder * new_encoder)
{
	g_return_if_fail(old_encoder_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_encoder_ptr,
		GST_MFX_MINI_OBJECT(new_encoder));
}

gboolean
gst_mfx_encoder_set_bitrate(GstMfxEncoder * encoder, mfxU16 bitrate)
{
	encoder->bitrate = bitrate;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_preset(GstMfxEncoder * encoder, GstMfxEncoderPreset preset)
{
	encoder->preset = preset;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_rate_control(GstMfxEncoder * encoder, GstMfxRateControl rc_method)
{
	encoder->rc_method = rc_method;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_idr_interval(GstMfxEncoder * encoder, mfxU16 idr_interval)
{
	encoder->idr_interval = idr_interval;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_gop_size(GstMfxEncoder * encoder, mfxU16 gop_size)
{
	encoder->gop_size = gop_size;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_gop_refdist(GstMfxEncoder * encoder, mfxU16 gop_refdist)
{
	encoder->gop_refdist = gop_refdist;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_num_references(GstMfxEncoder * encoder, mfxU16 num_refs)
{
	encoder->num_refs = num_refs;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_num_slices(GstMfxEncoder * encoder, mfxU16 num_slices)
{
	encoder->num_slices = num_slices;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_qpi(GstMfxEncoder * encoder, mfxU16 qpi)
{
	encoder->qpi = qpi;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_qpp(GstMfxEncoder * encoder, mfxU16 qpp)
{
	encoder->qpp = qpp;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_qpb(GstMfxEncoder * encoder, mfxU16 qpb)
{
	encoder->qpb = qpb;

	return TRUE;
}


GstMfxEncoderStatus
gst_mfx_encoder_start(GstMfxEncoder *encoder)
{
	mfxStatus sts = MFX_ERR_NONE;
	mfxFrameAllocRequest enc_request;
	mfxFrameAllocResponse enc_response;

	memset(&enc_request, 0, sizeof (mfxFrameAllocRequest));

	gst_mfx_encoder_set_input_params(encoder);

	/*sts = MFXVideoENCODE_Query(encoder->session, &encoder->params,
				&encoder->params);
	if (sts > 0)
		GST_WARNING("Incompatible video params detected %d", sts);*/

	sts = MFXVideoENCODE_QueryIOSurf(encoder->session, &encoder->params,
				&enc_request);
	if (sts < 0) {
		GST_ERROR("Unable to query encode allocation request %d", sts);
		return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
	}

	gst_mfx_task_set_request(encoder->encode_task, &enc_request);

	if (GST_VIDEO_INFO_FORMAT(&encoder->info) != GST_VIDEO_FORMAT_NV12) {
		encoder->filter = gst_mfx_filter_new_with_task(encoder->aggregator,
			encoder->encode_task, GST_MFX_TASK_VPP_OUT, encoder->mapped, encoder->mapped);

		enc_request.NumFrameSuggested += (1 - encoder->params.AsyncDepth);

		gst_mfx_filter_set_request(encoder->filter, &enc_request,
			GST_MFX_TASK_VPP_OUT);

        gst_mfx_filter_set_frame_info(encoder->filter, &encoder->info);

		gst_mfx_filter_set_format(encoder->filter, GST_VIDEO_FORMAT_NV12);

		if (!gst_mfx_filter_start(encoder->filter))
			return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;

		encoder->pool = gst_mfx_filter_get_pool(encoder->filter,
			GST_MFX_TASK_VPP_OUT);
		if (!encoder->pool)
			return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
	}
	/*else {
		sts = gst_mfx_task_frame_alloc(encoder->encode_task, &enc_request,
			&enc_response);
		if (MFX_ERR_NONE != sts) {
			return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
		}
	}*/

	sts = MFXVideoENCODE_Init(encoder->session, &encoder->params);
	if (sts < 0) {
		GST_ERROR("Error initializing the MFX video encoder %d", sts);
		return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;
	}

	if (!encoder->pool) {
		encoder->pool = gst_mfx_surface_pool_new_with_task(encoder->encode_task);
		if (!encoder->pool)
			return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
	}

	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

GstMfxEncoderStatus
gst_mfx_encoder_encode(GstMfxEncoder * encoder,
	GstVideoCodecFrame * frame)
{
	GstMapInfo minfo;
	GstMfxSurfaceProxy *proxy, *filter_proxy, *in_proxy;
	mfxFrameSurface1 *insurf;
	mfxSyncPoint syncp;
	mfxStatus sts = MFX_ERR_NONE;

	proxy = gst_video_codec_frame_get_user_data(frame);

	if (gst_mfx_task_has_type(encoder->encode_task, GST_MFX_TASK_VPP_OUT)) {
		gst_mfx_filter_process(encoder->filter, proxy, &filter_proxy);

		proxy = filter_proxy;
	}

	do {
		encoder->bs.Data = encoder->bitstream->data;

		insurf = gst_mfx_surface_proxy_get_frame_surface (proxy);
		sts = MFXVideoENCODE_EncodeFrameAsync(encoder->session,
			NULL, insurf, &encoder->bs, &syncp);

		if (MFX_WRN_DEVICE_BUSY == sts)
			g_usleep(500);
		else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			encoder->bitstream = g_byte_array_remove_range(encoder->bitstream, 0,
				encoder->bs.DataOffset);
			encoder->bs.DataOffset = 0;
			encoder->bs.MaxLength = encoder->bitstream->len +
				encoder->params.mfx.BufferSizeInKB * 1000;
			encoder->bitstream = g_byte_array_set_size(encoder->bs.Data,
				encoder->bs.MaxLength);
		}
	} while (MFX_WRN_DEVICE_BUSY == sts || MFX_ERR_MORE_SURFACE == sts);

	if (MFX_ERR_MORE_BITSTREAM == sts)
		return GST_MFX_ENCODER_STATUS_NO_BUFFER;
	else if (MFX_ERR_MORE_DATA == sts)
		return GST_MFX_ENCODER_STATUS_MORE_DATA;

	if (sts != MFX_ERR_NONE &&
		sts != MFX_ERR_MORE_BITSTREAM &&
		sts != MFX_WRN_VIDEO_PARAM_CHANGED) {
		GST_ERROR("Error during MFX encoding.");
		return GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN;
	}

	if (syncp) {
		do {
			sts = MFXVideoCORE_SyncOperation(encoder->session, syncp, 1000);
		} while (MFX_WRN_IN_EXECUTION == sts);

		frame->output_buffer = gst_buffer_copy(gst_buffer_new_wrapped(encoder->bs.Data,
            encoder->bs.MaxLength));
	}

	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

/**
* gst_mfx_encoder_set_property:
* @encoder: a #GstMfxEncoder
* @prop_id: the id of the property to change
* @value: the new value to set
*
* Update the requested property, designed by @prop_id, with the
* supplied @value. A @NULL value argument resets the property to its
* default value.
*
* Return value: a #GstMfxEncoderStatus
*/
static GstMfxEncoderStatus
set_property(GstMfxEncoder * encoder, gint prop_id, const GValue * value)
{
	GstMfxEncoderStatus status =
		GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;

	g_assert(value != NULL);

	/* Handle codec-specific properties */
	if (prop_id < 0) {
		GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS(encoder);

		if (klass->set_property) {
			status = klass->set_property(encoder, prop_id, value);
		}
		return status;
	}

	/* Handle common properties */
	switch (prop_id) {
	case GST_MFX_ENCODER_PROP_RATECONTROL:
		status = gst_mfx_encoder_set_rate_control(encoder,
			g_value_get_enum(value));
		break;
	case GST_MFX_ENCODER_PROP_BITRATE:
		status = gst_mfx_encoder_set_bitrate(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_IDR_INTERVAL:
		status = gst_mfx_encoder_set_idr_interval(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_GOP_SIZE:
		status = gst_mfx_encoder_set_gop_size(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_GOP_REFDIST:
		status = gst_mfx_encoder_set_gop_refdist(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_NUM_REFS:
		status = gst_mfx_encoder_set_num_references(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_NUM_SLICES:
		status = gst_mfx_encoder_set_num_slices(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_QPI:
		status = gst_mfx_encoder_set_qpi(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_QPP:
		status = gst_mfx_encoder_set_qpp(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_QPB:
		status = gst_mfx_encoder_set_qpb(encoder,
			g_value_get_uint(value));
		break;
	case GST_MFX_ENCODER_PROP_PRESET:
		status = gst_mfx_encoder_set_preset(encoder, g_value_get_enum(value));
		break;
	}
	return status;

	/* ERRORS */
error_operation_failed:
	{
		GST_ERROR("could not change codec state after encoding started");
		return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;
	}
}

GstMfxEncoderStatus
gst_mfx_encoder_set_property(GstMfxEncoder * encoder, gint prop_id,
	const GValue * value)
{
	GstMfxEncoderStatus status;
	GValue default_value = G_VALUE_INIT;

	g_return_val_if_fail(encoder != NULL,
		GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER);

	if (!value) {
		GParamSpec *const pspec = prop_find_pspec(encoder, prop_id);
		if (!pspec)
			goto error_invalid_property;

		g_value_init(&default_value, pspec->value_type);
		g_param_value_set_default(pspec, &default_value);
		value = &default_value;
	}

	status = set_property(encoder, prop_id, value);

	if (default_value.g_type)
		g_value_unset(&default_value);
	return status;

	/* ERRORS */
error_invalid_property:
	{
		GST_ERROR("unsupported property (%d)", prop_id);
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
	}
}

/* Checks video info */
static GstMfxEncoderStatus
check_video_info(GstMfxEncoder * encoder, const GstVideoInfo * vip)
{
	if (!vip->width || !vip->height)
		goto error_invalid_resolution;
	if (vip->fps_n < 0 || vip->fps_d <= 0)
		goto error_invalid_framerate;
	return GST_MFX_ENCODER_STATUS_SUCCESS;

	/* ERRORS */
error_invalid_resolution:
	{
		GST_ERROR("invalid resolution (%dx%d)", vip->width, vip->height);
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
	}
error_invalid_framerate:
	{
		GST_ERROR("invalid framerate (%d/%d)", vip->fps_n, vip->fps_d);
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
	}
}


/* Reconfigures the encoder with the new properties */
static GstMfxEncoderStatus
gst_mfx_encoder_reconfigure_internal(GstMfxEncoder * encoder)
{
	GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS(encoder);
	GstMfxEncoderStatus status;

	status = klass->reconfigure(encoder);
	if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
		return status;

	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

/**
* gst_mfx_encoder_set_codec_state:
* @encoder: a #GstMfxEncoder
* @state : a #GstVideoCodecState
*
* Notifies the encoder about the source surface properties. The
* accepted set of properties is: video resolution, colorimetry,
* pixel-aspect-ratio and framerate.
*
* This function is a synchronization point for codec configuration.
* This means that, at this point, the encoder is reconfigured to
* match the new properties and any other change beyond this point has
* zero effect.
*
* Return value: a #GstMfxEncoderStatus
*/
GstMfxEncoderStatus
gst_mfx_encoder_set_codec_state(GstMfxEncoder * encoder,
	GstVideoCodecState * state)
{
	GstMfxEncoderStatus status;

	g_return_val_if_fail(encoder != NULL,
		GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(state != NULL,
		GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER);

	if (!gst_video_info_is_equal(&state->info, &encoder->info)) {
		status = check_video_info(encoder, &state->info);
		if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
			return status;
		encoder->info = state->info;
	}
	return gst_mfx_encoder_reconfigure_internal(encoder);

	/* ERRORS */
error_operation_failed:
	{
		GST_ERROR("could not change codec state after encoding started");
		return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;
	}
}

GstMfxEncoderStatus
gst_mfx_encoder_get_codec_data (GstMfxEncoder * encoder,
    GstBuffer ** out_codec_data_ptr)
{
  GstMfxEncoderStatus ret = GST_MFX_ENCODER_STATUS_SUCCESS;
  GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS (encoder);

  *out_codec_data_ptr = NULL;
  if (!klass->get_codec_data)
    return GST_MFX_ENCODER_STATUS_SUCCESS;

  ret = klass->get_codec_data (encoder, out_codec_data_ptr);
  return ret;
}

/** Returns a GType for the #GstVaapiEncoderTune set */
GType
gst_mfx_encoder_preset_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue encoder_preset_values[] = {
    /* *INDENT-OFF* */
    { GST_MFX_ENCODER_PRESET_VERY_FAST,
      "Best speed", "very-fast" },
    { GST_MFX_ENCODER_PRESET_FASTER,
      "Faster", "faster" },
    { GST_MFX_ENCODER_PRESET_FAST,
      "Fast", "fast" },
    { GST_MFX_ENCODER_PRESET_MEDIUM,
      "Balanced", "medium" },
    { GST_MFX_ENCODER_PRESET_SLOW,
      "Slow", "slow" },
    { GST_MFX_ENCODER_PRESET_SLOWER,
      "Slower", "slower" },
    { GST_MFX_ENCODER_PRESET_VERY_SLOW,
      "Best quality", "very-slow" },
    { 0, NULL, NULL },
    /* *INDENT-ON* */
  };

  if (g_once_init_enter (&g_type)) {
    GType type =
        g_enum_register_static ("GstMfxEncoderPreset", encoder_preset_values);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}
