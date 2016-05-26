#include "sysdeps.h"
#include <va/va.h>

#include "gstmfxcompat.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxencoder_h264.h"
#include "gstmfxutils_h264.h"
#include "gstmfxutils_h264_priv.h"
#include "gstmfxsurfaceproxy.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Define the maximum IDR period */
#define MAX_IDR_PERIOD 512

/* Default CPB length (in milliseconds) */
#define DEFAULT_CPB_LENGTH 1500

/* Scale factor for CPB size (HRD cpb_size_scale: min = 4) */
#define SX_CPB_SIZE 4

/* Scale factor for bitrate (HRD bit_rate_scale: min = 6) */
#define SX_BITRATE 6

/* Define default rate control mode ("constant-qp") */
#define DEFAULT_RATECONTROL GST_MFX_RATECONTROL_CQP

/* Supported set of rate control methods, within this implementation */
#define SUPPORTED_RATECONTROLS                          \
	(GST_MFX_RATECONTROL_MASK (CQP)		|               \
	GST_MFX_RATECONTROL_MASK (CBR)		|               \
	GST_MFX_RATECONTROL_MASK (VBR)		|               \
	GST_MFX_RATECONTROL_MASK (AVBR)		|				\
	GST_MFX_RATECONTROL_MASK (LA_BRC)	|               \
	GST_MFX_RATECONTROL_MASK (ICQ)		|               \
	GST_MFX_RATECONTROL_MASK (LA_ICQ))

/* Determines the cpbBrNalFactor based on the supplied profile */
static guint
h264_get_cpb_nal_factor(GstMfxProfile profile)
{
	guint f;

	/* Table A-2 */
	switch (profile) {
	case GST_MFX_PROFILE_AVC_HIGH:
		f = 1500;
		break;
	case GST_MFX_PROFILE_AVC_HIGH10:
		f = 3600;
		break;
	case GST_MFX_PROFILE_AVC_HIGH_422:
		f = 4800;
		break;
	case GST_MFX_PROFILE_AVC_MULTIVIEW_HIGH:
	case GST_MFX_PROFILE_AVC_STEREO_HIGH:
		f = 1500;                 /* H.10.2.1 (r) */
		break;
	default:
		f = 1200;
		break;
	}
	return f;
}

/* ------------------------------------------------------------------------- */
/* --- H.264 Encoder                                                     --- */
/* ------------------------------------------------------------------------- */

#define GST_MFX_ENCODER_H264_CAST(encoder) \
	((GstMfxEncoderH264 *)(encoder))

struct _GstMfxEncoderH264
{
	GstMfxEncoder parent_instance;

	GstMfxProfile profile;
	GstMfxLevelH264 level;
	guint8 profile_idc;
	guint8 max_profile_idc;
	guint8 level_idc;
	guint32 num_bframes;
	guint32 mb_width;
	guint32 mb_height;
	gboolean use_cabac;
	GstClockTime cts_offset;
	gboolean config_changed;

	guint bitrate_bits;           // bitrate (bits)
	guint cpb_length;             // length of CPB buffer (ms)
	guint cpb_length_bits;        // length of CPB buffer (bits)

	/* H264 specific coding options */
	mfxU16 la_depth;
	mfxU16 max_slice_size;
	mfxU16 max_frame_size;
	mfxU16 bitrate_limit;
	mfxU16 int_ref_type;
	mfxU16 int_ref_cycle_size;
	mfxU16 int_ref_qp_delta;

	mfxU16 mbbrc;
	mfxU16 extbrc;
	mfxU16 trellis;
	mfxU16 b_strategy;
	mfxU16 adaptive_i;
	mfxU16 adaptive_b;

	mfxU16 single_sei_nal_unit;
	mfxU16 recovery_point_sei;
	mfxU16 max_dec_frame_buffering;
	mfxU16 look_ahead_downsampling;
};

/* Check target decoder constraints */
static gboolean
ensure_profile_limits(GstMfxEncoderH264 * encoder)
{
	GstMfxProfile profile;

	if (!encoder->max_profile_idc)
		return FALSE;

	GST_WARNING("lowering coding tools to meet target decoder constraints");

	profile = GST_MFX_PROFILE_AVC_HIGH;

	/* Try Main profile coding tools */
	if (encoder->max_profile_idc < 100) {
		profile = GST_MFX_PROFILE_AVC_MAIN;
	}

	/* Try Constrained Baseline profile coding tools */
	if (encoder->max_profile_idc < 77) {
		if (encoder->num_bframes)
			profile = GST_MFX_PROFILE_AVC_BASELINE;
		else
			profile = GST_MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
	}

	if (profile) {
		encoder->profile = profile;
		encoder->profile_idc = encoder->max_profile_idc;
	}
	return TRUE;
}

/* Derives the level from the currently set limits */
static gboolean
ensure_level(GstMfxEncoderH264 * encoder)
{
	const guint cpb_factor = h264_get_cpb_nal_factor(encoder->profile);
	const GstMfxH264LevelLimits *limits_table;
	guint i, num_limits, PicSizeMbs, MaxDpbMbs, MaxMBPS;

	PicSizeMbs = encoder->mb_width * encoder->mb_height;
	MaxDpbMbs = PicSizeMbs * ((encoder->num_bframes) ? 2 : 1);
	MaxMBPS = gst_util_uint64_scale_int_ceil(PicSizeMbs,
		GST_MFX_ENCODER_FPS_N(encoder), GST_MFX_ENCODER_FPS_D(encoder));

	limits_table = gst_mfx_utils_h264_get_level_limits_table(&num_limits);
	for (i = 0; i < num_limits; i++) {
		const GstMfxH264LevelLimits *const limits = &limits_table[i];
		if (PicSizeMbs <= limits->MaxFS &&
			MaxDpbMbs <= limits->MaxDpbMbs &&
			MaxMBPS <= limits->MaxMBPS && (!encoder->bitrate_bits
			|| encoder->bitrate_bits <= (limits->MaxBR * cpb_factor)) &&
			(!encoder->cpb_length_bits ||
			encoder->cpb_length_bits <= (limits->MaxCPB * cpb_factor)))
			break;
	}
	if (i == num_limits)
		goto error_unsupported_level;

	encoder->level = limits_table[i].level;
	encoder->level_idc = limits_table[i].level_idc;

	return TRUE;

	/* ERRORS */
error_unsupported_level:
	{
		GST_ERROR("failed to find a suitable level matching codec config");
		return FALSE;
	}
}

/* Normalizes bitrate (and CPB size) for HRD conformance */
static void
ensure_bitrate_hrd(GstMfxEncoderH264 * encoder)
{
	GstMfxEncoder *const base_encoder = GST_MFX_ENCODER_CAST(encoder);
	guint bitrate, cpb_size;

	if (!base_encoder->bitrate) {
		encoder->bitrate_bits = 0;
		return;
	}

	/* Round down bitrate. This is a hard limit mandated by the user */
	g_assert(SX_BITRATE >= 6);
	bitrate = (base_encoder->bitrate * 1000) & ~((1U << SX_BITRATE) - 1);
	if (bitrate != encoder->bitrate_bits) {
		GST_DEBUG("HRD bitrate: %u bits/sec", bitrate);
		encoder->bitrate_bits = bitrate;
		encoder->config_changed = TRUE;
	}

	/* Round up CPB size. This is an HRD compliance detail */
	g_assert(SX_CPB_SIZE >= 4);
	cpb_size = gst_util_uint64_scale(bitrate, encoder->cpb_length, 1000) &
		~((1U << SX_CPB_SIZE) - 1);
	if (cpb_size != encoder->cpb_length_bits) {
		GST_DEBUG("HRD CPB size: %u bits", cpb_size);
		encoder->cpb_length_bits = cpb_size;
		encoder->config_changed = TRUE;
	}
}

/* Estimates a good enough bitrate if none was supplied */
static void
ensure_bitrate(GstMfxEncoderH264 * encoder)
{
	GstMfxEncoder *const base_encoder = GST_MFX_ENCODER_CAST(encoder);

	/* Default compression: 48 bits per macroblock in "high-compression" mode */
	switch (GST_MFX_ENCODER_RATE_CONTROL(encoder)) {
	case GST_MFX_RATECONTROL_CBR:
	case GST_MFX_RATECONTROL_VBR:
	case GST_MFX_RATECONTROL_AVBR:
	case GST_MFX_RATECONTROL_LA_BRC:
		if (!base_encoder->bitrate) {
			/* According to the literature and testing, CABAC entropy coding
			mode could provide for +10% to +18% improvement in general,
			thus estimating +15% here ; and using adaptive 8x8 transforms
			in I-frames could bring up to +10% improvement. */
			guint bits_per_mb = 48;
			if (!encoder->use_cabac)
				bits_per_mb += (bits_per_mb * 15) / 100;

			base_encoder->bitrate =
				encoder->mb_width * encoder->mb_height * bits_per_mb *
				GST_MFX_ENCODER_FPS_N(encoder) /
				GST_MFX_ENCODER_FPS_D(encoder) / 1000;
			GST_INFO("target bitrate computed to %u kbps", base_encoder->bitrate);
		}
		break;
	default:
		base_encoder->bitrate = 0;
		break;
	}
	ensure_bitrate_hrd(encoder);
}

/* Constructs profile and level information based on user-defined limits */
static GstMfxEncoderStatus
ensure_profile_and_level(GstMfxEncoderH264 * encoder)
{
	const GstMfxProfile profile = encoder->profile;
	const GstMfxLevelH264 level = encoder->level;

	if (!ensure_profile_limits(encoder))
		return GST_MFX_ENCODER_STATUS_ERROR_UNSUPPORTED_PROFILE;

	/* Ensure bitrate if not set already and derive the right level to use */
	ensure_bitrate(encoder);
	if (!ensure_level(encoder))
		return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;

	if (encoder->profile != profile || encoder->level != level) {
		GST_DEBUG("selected %s profile at level %s",
			gst_mfx_utils_h264_get_profile_string(encoder->profile),
			gst_mfx_utils_h264_get_level_string(encoder->level));
		encoder->config_changed = TRUE;
	}

	return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static GstMfxEncoderStatus
gst_mfx_encoder_h264_reconfigure (GstMfxEncoder * base_encoder)
{
    GstMfxEncoderH264 *const encoder =
        GST_MFX_ENCODER_H264_CAST (base_encoder);
    GstVideoInfo *const vip = GST_MFX_ENCODER_VIDEO_INFO (encoder);
    GstMfxEncoderStatus status;
    guint mb_width, mb_height;

    mb_width = (GST_MFX_ENCODER_WIDTH (encoder) + 15) / 16;
    mb_height = (GST_MFX_ENCODER_HEIGHT (encoder) + 15) / 16;
    if (mb_width != encoder->mb_width || mb_height != encoder->mb_height) {
        GST_DEBUG ("resolution: %dx%d", GST_MFX_ENCODER_WIDTH (encoder),
            GST_MFX_ENCODER_HEIGHT (encoder));
        encoder->mb_width = mb_width;
        encoder->mb_height = mb_height;
        encoder->config_changed = TRUE;
    }


    status = ensure_profile_and_level (encoder);
    if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
        return status;

    return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static gboolean
gst_mfx_encoder_h264_init(GstMfxEncoder * base_encoder)
{
	GstMfxEncoderH264 *const encoder =
		GST_MFX_ENCODER_H264_CAST(base_encoder);

	base_encoder->codec = MFX_CODEC_AVC;

	base_encoder->extco.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
	base_encoder->extco.Header.BufferSz = sizeof(base_encoder->extco);

	base_encoder->extco.CAVLC = !encoder->use_cabac ? MFX_CODINGOPTION_ON
		: MFX_CODINGOPTION_UNKNOWN;

	base_encoder->extco.PicTimingSEI = base_encoder->pic_timing_sei ?
		MFX_CODINGOPTION_ON : MFX_CODINGOPTION_UNKNOWN;

	if (base_encoder->rdo >= 0)
		base_encoder->extco.RateDistortionOpt = base_encoder->rdo > 0 ?
			MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;

	//base_encoder->extco.NalHrdConformance = encoder->strict_std_compliance ?
		//MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;

	if (encoder->single_sei_nal_unit >= 0)
		base_encoder->extco.SingleSeiNalUnit = encoder->single_sei_nal_unit ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
	if (encoder->recovery_point_sei >= 0)
		base_encoder->extco.RecoveryPointSEI = encoder->recovery_point_sei ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
	base_encoder->extco.MaxDecFrameBuffering = encoder->max_dec_frame_buffering;

	base_encoder->extparam_internal[base_encoder->nb_extparam_internal++] = (mfxExtBuffer *)&base_encoder->extco;

	base_encoder->extco2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
	base_encoder->extco2.Header.BufferSz = sizeof(base_encoder->extco2);

	if (encoder->int_ref_type)
		base_encoder->extco2.IntRefType = encoder->int_ref_type;
	if (encoder->int_ref_cycle_size)
		base_encoder->extco2.IntRefCycleSize = encoder->int_ref_cycle_size;
	if (encoder->int_ref_qp_delta != INT16_MIN)
		base_encoder->extco2.IntRefQPDelta = encoder->int_ref_qp_delta;

	if (encoder->bitrate_limit >= 0)
		base_encoder->extco2.BitrateLimit = encoder->bitrate_limit ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
	if (encoder->mbbrc >= 0)
		base_encoder->extco2.MBBRC = encoder->mbbrc ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
	if (encoder->extbrc >= 0)
		base_encoder->extco2.ExtBRC = encoder->extbrc ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;

	if (encoder->max_frame_size >= 0)
		base_encoder->extco2.MaxFrameSize = encoder->max_frame_size;
	if (encoder->max_slice_size >= 0)
		base_encoder->extco2.MaxSliceSize = encoder->max_slice_size;

	base_encoder->extco2.Trellis = encoder->trellis;

	if (encoder->b_strategy >= 0)
		base_encoder->extco2.BRefType = encoder->b_strategy ? MFX_B_REF_PYRAMID : MFX_B_REF_OFF;
	if (encoder->adaptive_i >= 0)
		base_encoder->extco2.AdaptiveI = encoder->adaptive_i ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
	if (encoder->adaptive_b >= 0)
		base_encoder->extco2.AdaptiveB = encoder->adaptive_b ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;


	base_encoder->extparam_internal[base_encoder->nb_extparam_internal++] = (mfxExtBuffer *)&base_encoder->extco2;

	base_encoder->extco2.LookAheadDS = encoder->look_ahead_downsampling;

	return TRUE;
}

static void
gst_mfx_encoder_h264_finalize(GstMfxEncoder * base_encoder)
{
	/*free private buffers */
	GstMfxEncoderH264 *const encoder =
		GST_MFX_ENCODER_H264_CAST(base_encoder);

}

/* Generate "codec-data" buffer */
static GstMfxEncoderStatus
gst_mfx_encoder_h264_get_codec_data(GstMfxEncoder * base_encoder,
	GstBuffer ** out_buffer_ptr)
{
	GstMfxEncoderH264 *const encoder =
		GST_MFX_ENCODER_H264_CAST(base_encoder);
	GstBuffer *buffer;
	guint8 *codec_data;
	mfxStatus sts;
	guint8 sps_data[128], pps_data[128];

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

	mfxExtCodingOption3 co3 = {
		.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3,
		.Header.BufferSz = sizeof(co3),
	};

	mfxExtBuffer *ext_buffers[] = {
		(mfxExtBuffer*)&extradata,
		(mfxExtBuffer*)&co,
		(mfxExtBuffer*)&co2,
		(mfxExtBuffer*)&co3,
	};

	base_encoder->params.ExtParam = ext_buffers;
	base_encoder->params.NumExtParam = 4;

	sts = MFXVideoENCODE_GetVideoParam(base_encoder->session, &base_encoder->params);
	if (sts < 0)
		return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;

	codec_data = (guint8 *)g_slice_alloc(extradata.SPSBufSize + extradata.PPSBufSize);
	if (!codec_data)
		goto error_alloc_buffer;

	memcpy(codec_data, sps_data, extradata.SPSBufSize);
	memcpy(codec_data + extradata.SPSBufSize, pps_data, extradata.PPSBufSize);

	buffer = gst_buffer_new_wrapped(codec_data,
		extradata.SPSBufSize + extradata.PPSBufSize);
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
		encoder->max_slice_size = g_value_get_uint(value);
		break;
	case GST_MFX_ENCODER_H264_PROP_LA_DEPTH:
		encoder->la_depth = g_value_get_uint(value);
		break;
	case GST_MFX_ENCODER_H264_PROP_CABAC:
		encoder->use_cabac = g_value_get_boolean(value);
		break;
	case GST_MFX_ENCODER_H264_PROP_CPB_LENGTH:
		encoder->cpb_length = g_value_get_uint(value);
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
	* GstMfxEncoderH264:max-slice-size:
	*
	* Maximum encoded slice size in bytes.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_MAX_SLICE_SIZE,
		g_param_spec_uint("max-slice-size",
		"Max slice size", "Maximum encoded slice size in bytes", 0, 10, 0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoderH264:la-depth:
	*
	* Depth of look ahead in number frames.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_LA_DEPTH,
		g_param_spec_uint("la-depth",
		"Lookahead depth", "Depth of look ahead in number frames", 1, 100, 1,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoderH264:cabac:
	*
	* Enable CABAC entropy coding mode for improved compression ratio,
	* at the expense that the minimum target profile is Main. Default
	* is CAVLC entropy coding mode.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_CABAC,
		g_param_spec_boolean("cabac",
		"Enable CABAC",
		"Enable CABAC entropy coding mode",
		FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	* GstMfxEncoderH264:cpb-length:
	*
	* The size of the CPB buffer in milliseconds.
	*/
	GST_MFX_ENCODER_PROPERTIES_APPEND(props,
		GST_MFX_ENCODER_H264_PROP_CPB_LENGTH,
		g_param_spec_uint("cpb-length",
		"CPB Length", "Length of the CPB buffer in milliseconds",
		1, 10000, DEFAULT_CPB_LENGTH,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	return props;
}

/**
* gst_mfx_encoder_h264_set_max_profile:
* @encoder: a #GstMfxEncoderH264
* @profile: an H.264 #GstMfxProfile
*
* Notifies the @encoder to use coding tools from the supplied
* @profile at most.
*
* This means that if the minimal profile derived to
* support the specified coding tools is greater than this @profile,
* then an error is returned when the @encoder is configured.
*
* Return value: %TRUE on success
*/
gboolean
gst_mfx_encoder_h264_set_max_profile(GstMfxEncoderH264 * encoder,
	GstMfxProfile profile)
{
	guint8 profile_idc;

	g_return_val_if_fail(encoder != NULL, FALSE);
	g_return_val_if_fail(profile != GST_MFX_PROFILE_UNKNOWN, FALSE);

	if (gst_mfx_profile_get_codec(profile) != MFX_CODEC_AVC)
		return FALSE;

	profile_idc = gst_mfx_utils_h264_get_profile_idc(profile);
	if (!profile_idc)
		return FALSE;

	encoder->max_profile_idc = profile_idc;
	return TRUE;
}

/**
* gst_mfx_encoder_h264_get_profile_and_level:
* @encoder: a #GstMfxEncoderH264
* @out_profile_ptr: return location for the #GstMfxProfile
* @out_level_ptr: return location for the #GstMfxLevelH264
*
* Queries the H.264 @encoder for the active profile and level. That
* information is only constructed and valid after the encoder is
* configured, i.e. after the gst_mfx_encoder_set_codec_state()
* function is called.
*
* Return value: %TRUE on success
*/
gboolean
gst_mfx_encoder_h264_get_profile_and_level(GstMfxEncoderH264 * encoder,
	GstMfxProfile * out_profile_ptr, GstMfxLevelH264 * out_level_ptr)
{
	g_return_val_if_fail(encoder != NULL, FALSE);

	if (!encoder->profile || !encoder->level)
		return FALSE;

	if (out_profile_ptr)
		*out_profile_ptr = encoder->profile;
	if (out_level_ptr)
		*out_level_ptr = encoder->level;
	return TRUE;
}
