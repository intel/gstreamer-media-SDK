#include "sysdeps.h"
#include "gstmfxutils_h264.h"

struct map
{
	guint value;
	const gchar *name;
};

/* Profile string map */
static const struct map gst_mfx_h264_profile_map[] = {
	{ MFX_PROFILE_AVC_CONSTRAINED_BASELINE, "constrained-baseline" },
	{ MFX_PROFILE_AVC_BASELINE, "baseline" },
	{ MFX_PROFILE_AVC_MAIN, "main" },
	{ MFX_PROFILE_AVC_EXTENDED, "extended" },
	{ MFX_PROFILE_AVC_HIGH, "high" },
	{ MFX_PROFILE_AVC_HIGH_422, "high-4:2:2" },
	{ 0, NULL }
};

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

/** Returns MFX profile from a string representation */
mfxU16
gst_mfx_utils_h264_get_profile_from_string(const gchar * str)
{
	const struct map *const m = map_lookup_name(gst_mfx_h264_profile_map, str);

	return m ? m->value : 0;
}

/** Returns a string representation for the supplied H.264 profile */
const gchar *
gst_mfx_utils_h264_get_profile_string(mfxU16 profile)
{
	const struct map *const m =
		map_lookup_value(gst_mfx_h264_profile_map, profile);

	return m ? m->name : NULL;
}
