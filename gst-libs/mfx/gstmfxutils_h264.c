#include "sysdeps.h"
#include <gst/codecparsers/gsth264parser.h>
#include "gstmfxutils_h264_priv.h"

struct map
{
	guint value;
	const gchar *name;
};

/* Profile string map */
static const struct map gst_mfx_h264_profile_map[] = {
	/* *INDENT-OFF* */
	{ GST_MFX_PROFILE_AVC_CONSTRAINED_BASELINE, "constrained-baseline" },
	{ GST_MFX_PROFILE_AVC_BASELINE, "baseline" },
	{ GST_MFX_PROFILE_AVC_MAIN, "main" },
	{ GST_MFX_PROFILE_AVC_EXTENDED, "extended" },
	{ GST_MFX_PROFILE_AVC_HIGH, "high" },
	{ GST_MFX_PROFILE_AVC_HIGH_422, "high-4:2:2" },
	{ 0, NULL }
	/* *INDENT-ON* */
};

/* Level string map */
static const struct map gst_mfx_h264_level_map[] = {
	/* *INDENT-OFF* */
	{ MFX_LEVEL_AVC_1, "1" },
	{ MFX_LEVEL_AVC_1b, "1b" },
	{ MFX_LEVEL_AVC_11, "1.1" },
	{ MFX_LEVEL_AVC_12, "1.2" },
	{ MFX_LEVEL_AVC_13, "1.3" },
	{ MFX_LEVEL_AVC_2, "2" },
	{ MFX_LEVEL_AVC_21, "2.1" },
	{ MFX_LEVEL_AVC_22, "2.2" },
	{ MFX_LEVEL_AVC_3, "3" },
	{ MFX_LEVEL_AVC_31, "3.1" },
	{ MFX_LEVEL_AVC_32, "3.2" },
	{ MFX_LEVEL_AVC_4, "4" },
	{ MFX_LEVEL_AVC_41, "4.1" },
	{ MFX_LEVEL_AVC_42, "4.2" },
	{ MFX_LEVEL_AVC_5, "5" },
	{ MFX_LEVEL_AVC_51, "5.1" },
	{ MFX_LEVEL_AVC_52, "5.2" },
	{ 0, NULL }
	/* *INDENT-ON* */
};

/* Table A-1 - Level limits */
/* *INDENT-OFF* */
static const GstMfxH264LevelLimits gst_mfx_h264_level_limits[] = {
	/* level                     idc   MaxMBPS   MaxFS MaxDpbMbs  MaxBR MaxCPB */
	{ MFX_LEVEL_AVC_1, 10, 1485, 99, 396, 64, 175 },
	{ MFX_LEVEL_AVC_1b, 11, 1485, 99, 396, 128, 350 },
	{ MFX_LEVEL_AVC_11, 11, 3000, 396, 900, 192, 500 },
	{ MFX_LEVEL_AVC_12, 12, 6000, 396, 2376, 384, 1000 },
	{ MFX_LEVEL_AVC_13, 13, 11880, 396, 2376, 768, 2000 },
	{ MFX_LEVEL_AVC_2, 20, 11880, 396, 2376, 2000, 2000 },
	{ MFX_LEVEL_AVC_21, 21, 19800, 792, 4752, 4000, 4000 },
	{ MFX_LEVEL_AVC_22, 22, 20250, 1620, 8100, 4000, 4000 },
	{ MFX_LEVEL_AVC_3, 30, 40500, 1620, 8100, 10000, 10000 },
	{ MFX_LEVEL_AVC_31, 31, 108000, 3600, 18000, 14000, 14000 },
	{ MFX_LEVEL_AVC_32, 32, 216000, 5120, 20480, 20000, 20000 },
	{ MFX_LEVEL_AVC_4, 40, 245760, 8192, 32768, 20000, 25000 },
	{ MFX_LEVEL_AVC_41, 41, 245760, 8192, 32768, 50000, 62500 },
	{ MFX_LEVEL_AVC_42, 42, 522240, 8704, 34816, 50000, 62500 },
	{ MFX_LEVEL_AVC_5, 50, 589824, 22080, 110400, 135000, 135000 },
	{ MFX_LEVEL_AVC_51, 51, 983040, 36864, 184320, 240000, 240000 },
	{ MFX_LEVEL_AVC_52, 52, 2073600, 36864, 184320, 240000, 240000 },
	{ 0, }
};
/* *INDENT-ON* */

/* Lookup value in map */
static const struct map *
map_lookup_value(const struct map *m, guint value)
{
	g_return_val_if_fail(m != NULL, NULL);

	for (; m->name != NULL; m++) {
		if (m->value == value)
			return m;
	}
	return NULL;
}

/* Lookup name in map */
static const struct map *
map_lookup_name(const struct map *m, const gchar * name)
{
	g_return_val_if_fail(m != NULL, NULL);

	if (!name)
		return NULL;

	for (; m->name != NULL; m++) {
		if (strcmp(m->name, name) == 0)
			return m;
	}
	return NULL;
}

/** Returns a relative score for the supplied GstMfxProfile */
guint
gst_mfx_utils_h264_get_profile_score(GstMfxProfile profile)
{
	const struct map *const m =
		map_lookup_value(gst_mfx_h264_profile_map, profile);

	return m ? 1 + (m - gst_mfx_h264_profile_map) : 0;
}

/** Returns H.264 profile_idc value from GstMfxProfile */
guint8
gst_mfx_utils_h264_get_profile_idc(GstMfxProfile profile)
{
	guint8 profile_idc;

	switch (profile) {
	case GST_MFX_PROFILE_AVC_BASELINE:
	case GST_MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
		profile_idc = GST_H264_PROFILE_BASELINE;
		break;
	case GST_MFX_PROFILE_AVC_MAIN:
		profile_idc = GST_H264_PROFILE_MAIN;
		break;
	case GST_MFX_PROFILE_AVC_EXTENDED:
		profile_idc = GST_H264_PROFILE_EXTENDED;
		break;
	case GST_MFX_PROFILE_AVC_HIGH:
		profile_idc = GST_H264_PROFILE_HIGH;
		break;
	case GST_MFX_PROFILE_AVC_HIGH_422:
		profile_idc = GST_H264_PROFILE_HIGH_422;
		break;
	default:
		g_debug("unsupported GstMfxProfile value");
		profile_idc = 0;
		break;
	}
	return profile_idc;
}

/** Returns GstMfxProfile from a string representation */
GstMfxProfile
gst_mfx_utils_h264_get_profile_from_string(const gchar * str)
{
	const struct map *const m = map_lookup_name(gst_mfx_h264_profile_map, str);

	return m ? (GstMfxProfile)m->value : GST_MFX_PROFILE_UNKNOWN;
}

/** Returns a string representation for the supplied H.264 profile */
const gchar *
gst_mfx_utils_h264_get_profile_string(GstMfxProfile profile)
{
	const struct map *const m =
		map_lookup_value(gst_mfx_h264_profile_map, profile);

	return m ? m->name : NULL;
}

/** Returns a string representation for the supplied H.264 level */
const gchar *
gst_mfx_utils_h264_get_level_string(mfxU16 level)
{
	const struct map *const m =
		map_lookup_value(gst_mfx_h264_level_map, level);

    return m ? m->name : NULL;
}

/** Returns the Table A-1 specification */
const GstMfxH264LevelLimits *
gst_mfx_utils_h264_get_level_limits_table(guint * out_length_ptr)
{
	if (out_length_ptr)
		*out_length_ptr = G_N_ELEMENTS(gst_mfx_h264_level_limits) - 1;
	return gst_mfx_h264_level_limits;
}
