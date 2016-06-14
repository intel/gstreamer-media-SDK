#include "sysdeps.h"
#include <va/va.h>

#include "gstmfxcompat.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxencoder_h264.h"
#include "gstmfxutils_h264.h"
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

#define GST_MFX_ENCODER_H264_CAST(encoder) \
	((GstMfxEncoderH264 *)(encoder))

struct _GstMfxEncoderH264
{
	GstMfxEncoder parent_instance;
};

/* Estimates a good enough bitrate if none was supplied */
static void
ensure_bitrate(GstMfxEncoderH264 * encoder)
{
	GstMfxEncoder *const base_encoder = GST_MFX_ENCODER_CAST(encoder);

	/* Default compression: 48 bits per macroblock */
	switch (GST_MFX_ENCODER_RATE_CONTROL(encoder)) {
	case GST_MFX_RATECONTROL_CBR:
	case GST_MFX_RATECONTROL_VBR:
	case GST_MFX_RATECONTROL_AVBR:
	case GST_MFX_RATECONTROL_LA_BRC:
    case GST_MFX_RATECONTROL_LA_HRD:
		if (!base_encoder->bitrate) {
            guint mb_width = (GST_MFX_ENCODER_WIDTH (encoder) + 15) / 16;
            guint mb_height = (GST_MFX_ENCODER_HEIGHT (encoder) + 15) / 16;

            GST_DEBUG ("resolution: %dx%d", GST_MFX_ENCODER_WIDTH (encoder),
                GST_MFX_ENCODER_HEIGHT (encoder));

			/* According to the literature and testing, CABAC entropy coding
			mode could provide for +10% to +18% improvement in general,
			thus estimating +15% here ; and using adaptive 8x8 transforms
			in I-frames could bring up to +10% improvement. */
			guint bits_per_mb = 48;
			if (!base_encoder->use_cabac)
				bits_per_mb += (bits_per_mb * 15) / 100;

			base_encoder->bitrate =
				mb_width * mb_height * bits_per_mb *
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
gst_mfx_encoder_h264_reconfigure (GstMfxEncoder * base_encoder)
{
    GstMfxEncoderH264 *const encoder =
        GST_MFX_ENCODER_H264_CAST (base_encoder);
    GstMfxEncoderStatus status;

    if (base_encoder->profile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE ||
            base_encoder->profile == MFX_PROFILE_AVC_BASELINE) {
        base_encoder->gop_refdist = 1;
        /* CABAC is disabled when selecting baseline profile in Media SDK */
        base_encoder->use_cabac = FALSE;
    }

    if (base_encoder->gop_refdist == 1)
        base_encoder->b_strategy = GST_MFX_OPTION_OFF;

    /* Ensure bitrate if not set */
	ensure_bitrate(encoder);

    return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static gboolean
gst_mfx_encoder_h264_init(GstMfxEncoder * base_encoder)
{
	GstMfxEncoderH264 *const encoder =
		GST_MFX_ENCODER_H264_CAST(base_encoder);

	base_encoder->codec = MFX_CODEC_AVC;

	return TRUE;
}

static void
gst_mfx_encoder_h264_finalize(GstMfxEncoder * base_encoder)
{
}

/* Generate "codec-data" buffer */
static GstMfxEncoderStatus
gst_mfx_encoder_h264_get_codec_data(GstMfxEncoder * base_encoder,
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
gst_mfx_encoder_h264_set_property(GstMfxEncoder * base_encoder,
    gint prop_id, const GValue * value)
{
	GstMfxEncoderH264 *const encoder =
		GST_MFX_ENCODER_H264_CAST(base_encoder);

	switch (prop_id) {
	case GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE:
		base_encoder->max_slice_size = g_value_get_int(value);
		break;
	case GST_MFX_ENCODER_H264_PROP_LA_DEPTH:
		base_encoder->la_depth = g_value_get_uint(value);
		break;
	case GST_MFX_ENCODER_H264_PROP_CABAC:
		base_encoder->use_cabac = g_value_get_boolean(value);
		break;
    case GST_MFX_ENCODER_H264_PROP_TRELLIS:
		base_encoder->trellis = g_value_get_enum(value);
		break;
    case GST_MFX_ENCODER_H264_PROP_LOOKAHEAD_DS:
		base_encoder->look_ahead_downsampling = g_value_get_enum(value);
		break;
	default:
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
	}
	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

GST_MFX_ENCODER_DEFINE_CLASS_DATA(H264);

static inline const GstMfxEncoderClass *
gst_mfx_encoder_h264_class(void)
{
	static const GstMfxEncoderClass GstMfxEncoderH264Class = {
		GST_MFX_ENCODER_CLASS_INIT(H264, h264),
		.set_property = gst_mfx_encoder_h264_set_property,
		.get_codec_data = gst_mfx_encoder_h264_get_codec_data
	};
	return &GstMfxEncoderH264Class;
}

GstMfxEncoder *
gst_mfx_encoder_h264_new(GstMfxTaskAggregator * aggregator,
	GstVideoInfo * info, gboolean mapped)
{
	return gst_mfx_encoder_new(gst_mfx_encoder_h264_class(),
				aggregator, info, mapped);
}

/**
* gst_mfx_encoder_h264_get_default_properties:
*
* Determines the set of common and H.264 specific encoder properties.
* The caller owns an extra reference to the resulting array of
* #GstMfxEncoderPropInfo elements, so it shall be released with
* g_ptr_array_unref() after usage.
*
* Return value: the set of encoder properties for #GstMfxEncoderH264,
*   or %NULL if an error occurred.
*/
GPtrArray *
gst_mfx_encoder_h264_get_default_properties(void)
{
	const GstMfxEncoderClass *const klass = gst_mfx_encoder_h264_class();
	GPtrArray *props;

	props = gst_mfx_encoder_properties_get_default(klass);
	if (!props)
		return NULL;

	/**
	* GstMfxEncoderH264:max-slice-size
	*
	* Maximum encoded slice size in bytes.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE,
		g_param_spec_int("max-slice-size",
            "Maximum slice size", "Maximum encoded slice size in bytes",
            -1, G_MAXUINT16, -1,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoderH264:la-depth
	*
	* Depth of look ahead in number frames.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_LA_DEPTH,
		g_param_spec_uint("la-depth",
            "Lookahead depth", "Depth of lookahead in frames", 0, 100, 0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoderH264:cabac
	*
	* Enable CABAC entropy coding mode for improved compression ratio,
	* at the expense that the minimum target profile is Main. Default
	* is CABAC entropy coding mode.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_CABAC,
		g_param_spec_boolean("cabac",
            "Enable CABAC",
            "Enable CABAC entropy coding mode",
            TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
	* GstMfxEncoderH264:trellis
	*
	* Enable trellis quantization
	*/
    GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_TRELLIS,
		g_param_spec_enum("trellis",
            "Trellis quantization",
            "Enable trellis quantization",
            gst_mfx_encoder_trellis_get_type(), GST_MFX_ENCODER_TRELLIS_OFF,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


    /**
	* GstMfxEncoderH264:lookahead-ds
	*
	* Enable trellis Look ahead downsampling
	*/
    GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_LOOKAHEAD_DS,
		g_param_spec_enum("lookahead-ds",
            "Look ahead downsampling",
            "Look ahead downsampling",
            gst_mfx_encoder_lookahead_ds_get_type(),
            GST_MFX_ENCODER_LOOKAHEAD_DS_AUTO,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	return props;
}

gboolean
gst_mfx_encoder_h264_set_max_profile(GstMfxEncoder * encoder, mfxU16 profile)
{
	g_return_val_if_fail(encoder != NULL, FALSE);
	g_return_val_if_fail(profile != MFX_PROFILE_UNKNOWN, FALSE);

    encoder->profile = profile;
	return TRUE;
}
