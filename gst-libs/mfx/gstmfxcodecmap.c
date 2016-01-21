#include "gstmfxcodecmap.h"

typedef struct _GstMfxCodecMap                GstMfxCodecMap;

struct _GstMfxCodecMap {
	mfxU32 codec_format;
	const char *media_str;
	const gchar *profile_str;
};

/* Codecs */
static const GstMfxCodecMap gst_mfx_codecs[] = {
	{ MFX_CODEC_MPEG2, "video/mpeg, mpegversion=2", "simple" },
	{ MFX_CODEC_MPEG2, "video/mpeg, mpegversion=2", "main" },
	{ MFX_CODEC_MPEG2, "video/mpeg, mpegversion=2", "high" },
	{ MFX_CODEC_AVC, "video/mpeg, mpegversion=4", "simple" },
	{ MFX_CODEC_AVC, "video/mpeg, mpegversion=4", "advanced-simple" },
	{ MFX_CODEC_AVC, "video/x-h264", "baseline" },
	{ MFX_CODEC_AVC, "video/x-h264", "main" },
	{ MFX_CODEC_AVC, "video/x-h264", "high" },
	{ MFX_CODEC_VC1, "video/x-wmv, wmvversion=3", NULL },
	{ MFX_CODEC_VC1, "video/x-wmv, wmvversion=3 format=(string)WVC1", NULL },
	{ 0, }
};

mfxU32
gst_get_mfx_codec_from_caps(GstCaps * caps)
{
	const GstMfxCodecMap *m;
	GstStructure *structure = gst_caps_get_structure(caps, 0);
	const gchar *mimetype;
	gint codec_version;
	//const gchar *profile;

	mimetype = gst_structure_get_name(structure);
	//profile = gst_caps_to_string(caps);

	for (m = gst_mfx_codecs; m->codec_format; m++)
		if (!strcmp(m->media_str, mimetype)) {
			return m->codec_format;
        }

	return 0;
}
