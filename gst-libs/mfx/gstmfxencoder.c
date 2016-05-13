#include <mfxplugin.h>
#include "gstmfxencoder.h"
#include "gstmfxfilter.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxvideometa.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxtask.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxEncoder
{
	/*< private >*/
	GstMfxMiniObject        parent_instance;

	GstMfxTaskAggregator   *aggregator;
	GstMfxTask             *encode_task;
	GstMfxSurfacePool      *pool;
	GstMfxFilter           *filter;
	GByteArray             *bitstream;

	mfxSession              session;
	mfxVideoParam           params;
	mfxVideoParam			vpp_params;
	mfxBitstream            bs;
	mfxU32                  codec;
	gchar				   *plugin_uid;
	GstVideoInfo			info;

	/* Encoder params */
	mfxU16					target_usage;
	mfxU16					rc_method;
	mfxU16					target_bitrate;

};

static void
gst_mfx_encoder_finalize(GstMfxEncoder * encoder)
{
	g_byte_array_unref(encoder->bitstream);
	gst_mfx_task_aggregator_unref(encoder->aggregator);
	gst_mfx_task_replace(&encoder->encode_task, NULL);
	gst_mfx_surface_pool_unref(encoder->pool);

	MFXVideoENCODE_Close(encoder->session);
}

static void
gst_mfx_encoder_set_input_params(GstMfxEncoder * encoder)
{
	encoder->params.mfx.TargetUsage = encoder->target_usage;
	encoder->params.mfx.TargetKbps = encoder->target_bitrate;
	encoder->params.mfx.RateControlMethod = encoder->rc_method;

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

	if (encoder->codec == MFX_CODEC_HEVC &&
			!g_strcmp0(encoder->plugin_uid, "6fadc791a0c2eb479ab6dcd5ea9da347")) {
		encoder->params.mfx.FrameInfo.Width = GST_ROUND_UP_32(encoder->info.width);
		encoder->params.mfx.FrameInfo.Height = GST_ROUND_UP_32(encoder->info.height);
	}
	else {
		encoder->params.mfx.FrameInfo.Width = GST_ROUND_UP_16(encoder->info.width);
		encoder->params.mfx.FrameInfo.Height =
			(MFX_PICSTRUCT_PROGRESSIVE == encoder->params.mfx.FrameInfo.PicStruct) ?
			GST_ROUND_UP_16(encoder->info.height) :
			GST_ROUND_UP_32(encoder->info.height);
	}
}

static mfxStatus
gst_mfx_encoder_load_encoder_plugins(GstMfxEncoder * encoder, gchar ** out_uid)
{
	mfxPluginUID uid;
	mfxStatus sts;
	guint i, c;

	switch (encoder->codec) {
	case MFX_CODEC_HEVC:
	{
		gchar *plugin_uids[] = { "6fadc791a0c2eb479ab6dcd5ea9da347",
								 "2fca99749fdb49aeb121a5b63ef568f7",
								 NULL };

		for (i = 0; plugin_uids[i]; i++) {
			for (c = 0; c < sizeof(uid.Data); c++)
				sscanf(plugin_uids[i] + 2 * c, "%2hhx", uid.Data + c);
			sts = MFXVideoUSER_Load(encoder->session, &uid, 1);
			if (MFX_ERR_NONE == sts) {
				*out_uid = plugin_uids[i];
				break;
			}
		}
	}
		break;
	default:
		sts = MFX_ERR_NONE;
	}

	return sts;
}

static gboolean
gst_mfx_encoder_init(GstMfxEncoder * encoder,
GstMfxTaskAggregator * aggregator, mfxU32 codec, mfxU16 async_depth,
	GstVideoInfo * info, gboolean mapped)
{
	mfxStatus sts = MFX_ERR_NONE;

	encoder->info = *info;
	encoder->codec = encoder->params.mfx.CodecId = codec;
	encoder->params.AsyncDepth = async_depth;
	encoder->session = gst_mfx_task_aggregator_create_session(aggregator);

	sts = gst_mfx_encoder_load_encoder_plugins(encoder, &encoder->plugin_uid);
	if (sts < 0)
		return FALSE;

	if (encoder->plugin_uid == "2fca99749fdb49aeb121a5b63ef568f7")
		mapped = TRUE;

	encoder->params.IOPattern =
		mapped ? MFX_IOPATTERN_IN_SYSTEM_MEMORY : MFX_IOPATTERN_IN_VIDEO_MEMORY;
	encoder->aggregator = gst_mfx_task_aggregator_ref(aggregator);
	encoder->encode_task = gst_mfx_task_new_with_session(encoder->aggregator,
		encoder->session, GST_MFX_TASK_ENCODER);
	gst_mfx_task_aggregator_set_current_task(encoder->aggregator,
		encoder->encode_task);

	gst_mfx_encoder_set_input_params(encoder);

	encoder->bs.MaxLength = encoder->params.mfx.FrameInfo.Width *
		encoder->params.mfx.FrameInfo.Height * 4;
	encoder->bitstream = g_byte_array_sized_new(encoder->bs.MaxLength);
	if (!encoder->bitstream)
		return FALSE;

	return TRUE;
}

static inline const GstMfxMiniObjectClass *
gst_mfx_encoder_class(void)
{
	static const GstMfxMiniObjectClass GstMfxEncoderClass = {
		sizeof(GstMfxEncoder),
		(GDestroyNotify)gst_mfx_encoder_finalize
	};
	return &GstMfxEncoderClass;
}

GstMfxEncoder *
gst_mfx_encoder_new(GstMfxTaskAggregator * aggregator,
	mfxU32 codec, mfxU16 async_depth, GstVideoInfo * info, gboolean mapped)
{
	GstMfxEncoder *encoder;

	g_return_val_if_fail(aggregator != NULL, NULL);

	encoder = gst_mfx_mini_object_new0(gst_mfx_encoder_class());
	if (!encoder)
		goto error;

	if (!gst_mfx_encoder_init(encoder, aggregator, codec, async_depth,
		info, mapped))
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

void
gst_mfx_encoder_set_bitrate(GstMfxEncoder * encoder, mfxU16 bitrate)
{
	encoder->target_bitrate = bitrate;
}

gboolean
gst_mfx_encoder_set_target_usage(GstMfxEncoder * encoder, mfxU16 target_usage)
{
	encoder->target_usage = target_usage;

	return TRUE;
}

gboolean
gst_mfx_encoder_set_rate_control(GstMfxEncoder * encoder, mfxU16 rc_method)
{
	encoder->rc_method = rc_method;

	return TRUE;
}

GstMfxEncoderStatus
gst_mfx_encoder_start(GstMfxEncoder *encoder)
{
	mfxStatus sts = MFX_ERR_NONE;
	mfxFrameAllocRequest enc_request;
	mfxFrameAllocResponse enc_response;

	memset(&enc_request, 0, sizeof (mfxFrameAllocRequest));

	sts = MFXVideoENCODE_Query(encoder->session, &encoder->params,
				&encoder->params);
	if (sts > 0)
		GST_WARNING("Incompatible video params detected %d", sts);

	sts = MFXVideoENCODE_QueryIOSurf(encoder->session, &encoder->params,
				&enc_request);
	if (sts < 0) {
		GST_ERROR("Unable to query encode allocation request %d", sts);
		return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
	}

	gst_mfx_task_set_request(encoder->encode_task, &enc_request);

	if (GST_VIDEO_INFO_FORMAT(&encoder->info) != GST_VIDEO_FORMAT_NV12) {
		gboolean mapped = gst_mfx_task_has_mapped_surface(encoder->encode_task);
		encoder->filter = gst_mfx_filter_new_with_task(encoder->aggregator,
			encoder->encode_task, GST_MFX_TASK_VPP_OUT, mapped, mapped);

		enc_request.NumFrameSuggested =
			(enc_request.NumFrameSuggested - encoder->params.AsyncDepth) + 1;

		gst_mfx_filter_set_request(encoder->filter, &enc_request,
			GST_MFX_TASK_VPP_OUT);

		gst_mfx_filter_set_frame_info(encoder->filter, &encoder->info);

		gst_mfx_filter_set_format(encoder->filter, GST_VIDEO_FORMAT_NV12);

		if (!gst_mfx_filter_start(encoder->filter))
			return GST_MFX_ENCODER_STATUS_ERROR_INIT_FAILED;

		encoder->pool = gst_mfx_filter_get_pool(encoder->filter,
			GST_MFX_TASK_VPP_IN);
		if (!encoder->pool)
			return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
	}
	else {
		sts = gst_mfx_task_frame_alloc(encoder->encode_task, &enc_request,
			&enc_response);
		if (MFX_ERR_NONE != sts) {
			return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
		}
	}

	sts = MFXVideoENCODE_Init(encoder->session, &encoder->params);
	if (sts < 0) {
		GST_ERROR("Error initializing the MFX video encoder %d", sts);
		return GST_MFX_ENCODER_STATUS_ERROR_INIT_FAILED;
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
	GstMfxSurfaceProxy *proxy, *filter_proxy;
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
		return GST_MFX_ENCODER_STATUS_ERROR_NO_BUFFER;
	else if (MFX_ERR_MORE_DATA == sts)
		return GST_MFX_ENCODER_STATUS_SUCCESS;

	if (sts != MFX_ERR_NONE &&
		sts != MFX_ERR_MORE_BITSTREAM &&
		sts != MFX_WRN_VIDEO_PARAM_CHANGED) {
		GST_ERROR("Error during MFX decoding.");
		return GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN;
	}

	if (syncp) {
		do {
			sts = MFXVideoCORE_SyncOperation(encoder->session, syncp, 1000);
		} while (MFX_WRN_IN_EXECUTION == sts);

		if (!gst_buffer_map(frame->output_buffer, &minfo, GST_MAP_READ)) {
			GST_ERROR("Failed to map output buffer");
			return GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN;
		}

		minfo.data = encoder->bs.Data;
		minfo.size = encoder->bs.DataLength;

		gst_buffer_unmap(frame->output_buffer, &minfo);
	}

	return GST_MFX_ENCODER_STATUS_SUCCESS;
}
