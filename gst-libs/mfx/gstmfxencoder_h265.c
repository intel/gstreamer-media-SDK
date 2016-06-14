#include "sysdeps.h"
#include <va/va.h>
#include <mfxplugin.h>

#include "gstmfxcompat.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxencoder_h265.h"
#include "gstmfxutils_h265.h"
#include "gstmfxsurfaceproxy.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Define default rate control mode ("constant-qp") */
#define DEFAULT_RATECONTROL GST_MFX_RATECONTROL_CQP

/* Supported set of rate control methods, within this implementation */
#define SUPPORTED_RATECONTROLS                          \
	(GST_MFX_RATECONTROL_MASK (CQP)		|               \
	GST_MFX_RATECONTROL_MASK (CBR)		|               \
	GST_MFX_RATECONTROL_MASK (VBR)		|               \
	GST_MFX_RATECONTROL_MASK (AVBR)		|				\
	GST_MFX_RATECONTROL_MASK (LA_BRC)	|               \
	GST_MFX_RATECONTROL_MASK (LA_HRD)	|               \
	GST_MFX_RATECONTROL_MASK (ICQ)		|               \
	GST_MFX_RATECONTROL_MASK (LA_ICQ))

/* ------------------------------------------------------------------------- */
/* --- H.264 Encoder                                                     --- */
/* ------------------------------------------------------------------------- */

#define GST_MFX_ENCODER_H265_CAST(encoder) \
	((GstMfxEncoderH265 *)(encoder))

struct _GstMfxEncoderH265
{
	GstMfxEncoder parent_instance;
};

/* Estimates a good enough bitrate if none was supplied */
static void
ensure_bitrate(GstMfxEncoderH265 * encoder)
{
	GstMfxEncoder *const base_encoder = GST_MFX_ENCODER_CAST(encoder);

	switch (GST_MFX_ENCODER_RATE_CONTROL(encoder)) {
	case GST_MFX_RATECONTROL_CBR:
	case GST_MFX_RATECONTROL_VBR:
	case GST_MFX_RATECONTROL_AVBR:
	case GST_MFX_RATECONTROL_LA_BRC:
	case GST_MFX_RATECONTROL_LA_HRD:
		if (!base_encoder->bitrate) {
			guint luma_width = GST_ROUND_UP_32(GST_MFX_ENCODER_WIDTH(encoder));
			guint luma_height = GST_ROUND_UP_32(GST_MFX_ENCODER_HEIGHT(encoder));

			/* Fixme: Provide better estimation */
			/* Using a 1/6 compression ratio */
			/* 12 bits per pixel for yuv420 */
			base_encoder->bitrate =
				(luma_width * luma_height * 12 / 6) *
				GST_MFX_ENCODER_FPS_N(encoder) /
				GST_MFX_ENCODER_FPS_D(encoder) / 1000;
			GST_INFO("target bitrate computed to %u kbps", base_encoder->bitrate);
		}
		break;
	default:
		base_encoder->bitrate = 0;
		break;
	}
}

static GstMfxEncoderStatus
gst_mfx_encoder_h265_reconfigure(GstMfxEncoder * base_encoder)
{
	GstMfxEncoderH265 *const encoder =
		GST_MFX_ENCODER_H265_CAST(base_encoder);

	GST_DEBUG("resolution: %dx%d", GST_MFX_ENCODER_WIDTH(encoder),
		GST_MFX_ENCODER_HEIGHT(encoder));

	/* Ensure bitrate if not set */
	ensure_bitrate(encoder);

	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static mfxStatus
gst_mfx_encoder_load_hevc_plugin(GstMfxEncoder *encoder)
{
	mfxPluginUID uid;
	mfxStatus sts;
	guint i, c;

	gchar *plugin_uids[] = { "6fadc791a0c2eb479ab6dcd5ea9da347",
							 //"e5400a06c74d41f5b12d430bbaa23d0b",
							 "2fca99749fdb49aeb121a5b63ef568f7",
							 NULL };
	for (i = 0; plugin_uids[i]; i++) {
		for (c = 0; c < sizeof(uid.Data); c++)
			sscanf(plugin_uids[i] + 2 * c, "%2hhx", uid.Data + c);
		sts = MFXVideoUSER_Load(encoder->session, &uid, 1);
		if (MFX_ERR_NONE == sts) {
            encoder->plugin_uid = plugin_uids[i];
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
gst_mfx_encoder_h265_init(GstMfxEncoder * base_encoder)
{
	GstMfxEncoderH265 *const encoder =
		GST_MFX_ENCODER_H265_CAST(base_encoder);

	base_encoder->codec = MFX_CODEC_HEVC;

	return gst_mfx_encoder_load_hevc_plugin(base_encoder);
}

static void
gst_mfx_encoder_h265_finalize(GstMfxEncoder * base_encoder)
{
}

/* Generate "codec-data" buffer */
static GstMfxEncoderStatus
gst_mfx_encoder_h265_get_codec_data(GstMfxEncoder * base_encoder,
GstBuffer ** out_buffer_ptr)
{
	GstBuffer *buffer;
	guint8 *codec_data, *cur;
	mfxStatus sts;
	guint8 sps_data[128], pps_data[128];
	guint sps_size, pps_size, extradata_size;

	mfxExtCodingOptionSPSPPS extradata = {
		.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS,
		.Header.BufferSz = sizeof(extradata),
		.SPSBuffer = sps_data, .SPSBufSize = sizeof(sps_data),
		.PPSBuffer = pps_data, .PPSBufSize = sizeof(pps_data)
	};

	mfxExtCodingOption co = {
		.Header.BufferId = MFX_EXTBUFF_CODING_OPTION,
		.Header.BufferSz = sizeof(co),
	};

	mfxExtCodingOption2 co2 = {
		.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2,
		.Header.BufferSz = sizeof(co2),
	};

	mfxExtBuffer *ext_buffers[] = {
		(mfxExtBuffer*)&extradata,
		(mfxExtBuffer*)&co,
		(mfxExtBuffer*)&co2,
	};

	base_encoder->params.ExtParam = ext_buffers;
	base_encoder->params.NumExtParam = 3;

	sts = MFXVideoENCODE_GetVideoParam(base_encoder->session, &base_encoder->params);
	if (sts < 0)
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;

	sps_size = extradata.SPSBufSize - 4;
	pps_size = extradata.PPSBufSize - 4;
	extradata_size = sps_size + pps_size + 88;

	codec_data = (guint8 *)g_slice_alloc(extradata_size);
	if (!codec_data)
		goto error_alloc_buffer;

	cur = codec_data;

	*cur++ = 0x01;
	*cur++ = sps_data[5];
	*cur++ = sps_data[6];
	*cur++ = sps_data[7];
	*cur++ = 0xFF;
	*cur++ = 0xE1;

	/* Set sps size */
	*cur++ = (sps_size >> 8) & 0xFF;
	*cur++ = sps_size & 0xFF;

	memcpy(cur, sps_data + 4, sps_size);
	cur += sps_size;

	*cur++ = 0x01;

	/* Set pps size */
	*cur++ = (pps_size >> 8) & 0xFF;
	*cur++ = pps_size & 0xFF;

	memcpy(cur, pps_data + 4, pps_size);

	buffer =
		gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, codec_data,
		extradata_size, 0, extradata_size, NULL, NULL);
	if (!buffer)
		goto error_alloc_buffer;
	*out_buffer_ptr = buffer;

	return GST_MFX_ENCODER_STATUS_SUCCESS;

	/* ERRORS */
error_alloc_buffer:
	{
		GST_ERROR("failed to allocate codec-data buffer");
		return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
	}
}

static GstMfxEncoderStatus
gst_mfx_encoder_h265_set_property(GstMfxEncoder * base_encoder,
gint prop_id, const GValue * value)
{
	GstMfxEncoderH265 *const encoder =
		GST_MFX_ENCODER_H265_CAST(base_encoder);

	switch (prop_id) {
	case GST_MFX_ENCODER_H265_PROP_LA_DEPTH:
		base_encoder->la_depth = g_value_get_uint(value);
		break;
	case GST_MFX_ENCODER_H265_PROP_LOOKAHEAD_DS:
		base_encoder->look_ahead_downsampling = g_value_get_enum(value);
		break;
	default:
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
	}
	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

GST_MFX_ENCODER_DEFINE_CLASS_DATA(H265);

static inline const GstMfxEncoderClass *
gst_mfx_encoder_h265_class(void)
{
	static const GstMfxEncoderClass GstMfxEncoderH265Class = {
		GST_MFX_ENCODER_CLASS_INIT(H265, h265),
		.set_property = gst_mfx_encoder_h265_set_property,
		.get_codec_data = gst_mfx_encoder_h265_get_codec_data
	};
	return &GstMfxEncoderH265Class;
}

GstMfxEncoder *
gst_mfx_encoder_h265_new(GstMfxTaskAggregator * aggregator,
GstVideoInfo * info, gboolean mapped)
{
	return gst_mfx_encoder_new(gst_mfx_encoder_h265_class(),
		aggregator, info, mapped);
}

/**
* gst_mfx_encoder_h265_get_default_properties:
*
* Determines the set of common and H.264 specific encoder properties.
* The caller owns an extra reference to the resulting array of
* #GstMfxEncoderPropInfo elements, so it shall be released with
* g_ptr_array_unref() after usage.
*
* Return value: the set of encoder properties for #GstMfxEncoderH265,
*   or %NULL if an error occurred.
*/
GPtrArray *
gst_mfx_encoder_h265_get_default_properties(void)
{
	const GstMfxEncoderClass *const klass = gst_mfx_encoder_h265_class();
	GPtrArray *props;

	props = gst_mfx_encoder_properties_get_default(klass);
	if (!props)
		return NULL;

	/**
	* GstMfxEncoderH265:la-depth
	*
	* Depth of look ahead in number frames.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H265_PROP_LA_DEPTH,
		g_param_spec_uint("la-depth",
		"Lookahead depth", "Depth of lookahead in frames", 0, 100, 0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoderH265:lookahead-ds
	*
	* Look ahead downsampling
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H265_PROP_LOOKAHEAD_DS,
		g_param_spec_enum("lookahead-ds",
		"Look ahead downsampling",
		"Look ahead downsampling",
		gst_mfx_encoder_lookahead_ds_get_type(),
		GST_MFX_ENCODER_LOOKAHEAD_DS_AUTO,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	return props;
}

gboolean
gst_mfx_encoder_h265_set_max_profile(GstMfxEncoder * encoder, mfxU16 profile)
{
	g_return_val_if_fail(encoder != NULL, FALSE);
	g_return_val_if_fail(profile != MFX_PROFILE_UNKNOWN, FALSE);

	encoder->profile = profile;
	return TRUE;
}
