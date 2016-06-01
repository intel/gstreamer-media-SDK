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
	{ GST_MFX_LEVEL_H264_L1, "1" },
	{ GST_MFX_LEVEL_H264_L1b, "1b" },
	{ GST_MFX_LEVEL_H264_L1_1, "1.1" },
	{ GST_MFX_LEVEL_H264_L1_2, "1.2" },
	{ GST_MFX_LEVEL_H264_L1_3, "1.3" },
	{ GST_MFX_LEVEL_H264_L2, "2" },
	{ GST_MFX_LEVEL_H264_L2_1, "2.1" },
	{ GST_MFX_LEVEL_H264_L2_2, "2.2" },
	{ GST_MFX_LEVEL_H264_L3, "3" },
	{ GST_MFX_LEVEL_H264_L3_1, "3.1" },
	{ GST_MFX_LEVEL_H264_L3_2, "3.2" },
	{ GST_MFX_LEVEL_H264_L4, "4" },
	{ GST_MFX_LEVEL_H264_L4_1, "4.1" },
	{ GST_MFX_LEVEL_H264_L4_2, "4.2" },
	{ GST_MFX_LEVEL_H264_L5, "5" },
	{ GST_MFX_LEVEL_H264_L5_1, "5.1" },
	{ GST_MFX_LEVEL_H264_L5_2, "5.2" },
	{ 0, NULL }
	/* *INDENT-ON* */
};

/* Table A-1 - Level limits */
/* *INDENT-OFF* */
static const GstMfxH264LevelLimits gst_mfx_h264_level_limits[] = {
	/* level                     idc   MaxMBPS   MaxFS MaxDpbMbs  MaxBR MaxCPB */
	{ GST_MFX_LEVEL_H264_L1, 10, 1485, 99, 396, 64, 175 },
	{ GST_MFX_LEVEL_H264_L1b, 11, 1485, 99, 396, 128, 350 },
	{ GST_MFX_LEVEL_H264_L1_1, 11, 3000, 396, 900, 192, 500 },
	{ GST_MFX_LEVEL_H264_L1_2, 12, 6000, 396, 2376, 384, 1000 },
	{ GST_MFX_LEVEL_H264_L1_3, 13, 11880, 396, 2376, 768, 2000 },
	{ GST_MFX_LEVEL_H264_L2, 20, 11880, 396, 2376, 2000, 2000 },
	{ GST_MFX_LEVEL_H264_L2_1, 21, 19800, 792, 4752, 4000, 4000 },
	{ GST_MFX_LEVEL_H264_L2_2, 22, 20250, 1620, 8100, 4000, 4000 },
	{ GST_MFX_LEVEL_H264_L3, 30, 40500, 1620, 8100, 10000, 10000 },
	{ GST_MFX_LEVEL_H264_L3_1, 31, 108000, 3600, 18000, 14000, 14000 },
	{ GST_MFX_LEVEL_H264_L3_2, 32, 216000, 5120, 20480, 20000, 20000 },
	{ GST_MFX_LEVEL_H264_L4, 40, 245760, 8192, 32768, 20000, 25000 },
	{ GST_MFX_LEVEL_H264_L4_1, 41, 245760, 8192, 32768, 50000, 62500 },
	{ GST_MFX_LEVEL_H264_L4_2, 42, 522240, 8704, 34816, 50000, 62500 },
	{ GST_MFX_LEVEL_H264_L5, 50, 589824, 22080, 110400, 135000, 135000 },
	{ GST_MFX_LEVEL_H264_L5_1, 51, 983040, 36864, 184320, 240000, 240000 },
	{ GST_MFX_LEVEL_H264_L5_2, 52, 2073600, 36864, 184320, 240000, 240000 },
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

/** Returns GstMfxProfile from H.264 profile_idc value */
GstMfxProfile
gst_mfx_utils_h264_get_profile(guint8 profile_idc)
{
	GstMfxProfile profile;

	switch (profile_idc) {
	case GST_H264_PROFILE_BASELINE:
		profile = GST_MFX_PROFILE_AVC_BASELINE;
		break;
	case GST_H264_PROFILE_MAIN:
		profile = GST_MFX_PROFILE_AVC_MAIN;
		break;
	case GST_H264_PROFILE_EXTENDED:
		profile = GST_MFX_PROFILE_AVC_EXTENDED;
		break;
	case GST_H264_PROFILE_HIGH:
		profile = GST_MFX_PROFILE_AVC_HIGH;
		break;
	case GST_H264_PROFILE_HIGH_422:
		profile = GST_MFX_PROFILE_AVC_HIGH_422;
		break;
	default:
		g_debug("unsupported profile_idc value");
		profile = GST_MFX_PROFILE_UNKNOWN;
		break;
	}
	return profile;
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

/** Returns GstMfxLevelH264 from H.264 level_idc value */
GstMfxLevelH264
gst_mfx_utils_h264_get_level(guint8 level_idc)
{
	const GstMfxH264LevelLimits *llp;

	// Prefer Level 1.1 over level 1b
	if (G_UNLIKELY(level_idc == 11))
		return GST_MFX_LEVEL_H264_L1_1;

	for (llp = gst_mfx_h264_level_limits; llp->level != 0; llp++) {
		if (llp->level_idc == level_idc)
			return llp->level;
	}
	g_debug("unsupported level_idc value");
	return (GstMfxLevelH264)0;
}

/** Returns H.264 level_idc value from GstMfxLevelH264 */
guint8
gst_mfx_utils_h264_get_level_idc(GstMfxLevelH264 level)
{
	const GstMfxH264LevelLimits *const llp =
		gst_mfx_utils_h264_get_level_limits(level);

	return llp ? llp->level_idc : 0;
}

/** Returns GstMfxLevelH264 from a string representation */
GstMfxLevelH264
gst_mfx_utils_h264_get_level_from_string(const gchar * str)
{
	gint v, level_idc = 0;

	if (!str || !str[0])
		goto not_found;

	v = g_ascii_digit_value(str[0]);
	if (v < 0)
		goto not_found;
	level_idc = v * 10;

	switch (str[1]) {
	case '\0':
		break;
	case '.':
		v = g_ascii_digit_value(str[2]);
		if (v < 0 || str[3] != '\0')
			goto not_found;
		level_idc += v;
		break;
	case 'b':
		if (level_idc == 10 && str[2] == '\0')
			return GST_MFX_LEVEL_H264_L1b;
		// fall-trough
	default:
		goto not_found;
	}
	return gst_mfx_utils_h264_get_level(level_idc);

not_found:
	return (GstMfxLevelH264)0;
}

/** Returns a string representation for the supplied H.264 level */
const gchar *
gst_mfx_utils_h264_get_level_string(GstMfxLevelH264 level)
{
	if (level < GST_MFX_LEVEL_H264_L1 || level > GST_MFX_LEVEL_H264_L5_2)
		return NULL;
	return gst_mfx_h264_level_map[level - GST_MFX_LEVEL_H264_L1].name;
}

/** Returns level limits as specified in Table A-1 of the H.264 standard */
const GstMfxH264LevelLimits *
gst_mfx_utils_h264_get_level_limits(GstMfxLevelH264 level)
{
	if (level < GST_MFX_LEVEL_H264_L1 || level > GST_MFX_LEVEL_H264_L5_2)
		return NULL;
	return &gst_mfx_h264_level_limits[level - GST_MFX_LEVEL_H264_L1];
}

/** Returns the Table A-1 specification */
const GstMfxH264LevelLimits *
gst_mfx_utils_h264_get_level_limits_table(guint * out_length_ptr)
{
	if (out_length_ptr)
		*out_length_ptr = G_N_ELEMENTS(gst_mfx_h264_level_limits) - 1;
	return gst_mfx_h264_level_limits;
}