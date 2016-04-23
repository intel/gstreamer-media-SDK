#include "gstmfxcodecmap.h"

mfxU32
gst_get_mfx_codec_from_caps(GstCaps * caps)
{
	const gchar *mimetype;
	GstStructure *structure = gst_caps_get_structure(caps, 0);

	mimetype = gst_structure_get_name(structure);

	if (!g_strcmp0(mimetype, "video/x-h264")) {
		return MFX_CODEC_AVC;
	}
	else if (!g_strcmp0(mimetype, "video/x-wmv")) {
		return MFX_CODEC_VC1;
	}
	else if (!g_strcmp0(mimetype, "video/mpeg")) {
		gint mpegversion;

		gst_structure_get_int(structure, "mpegversion", &mpegversion);
		if (mpegversion == 2)
			return MFX_CODEC_MPEG2;
	}
	else if (!g_strcmp0(mimetype, "video/x-h265")) {
        return MFX_CODEC_HEVC;
    }
    else if (!g_strcmp0(mimetype, "image/jpeg")) {
		return MFX_CODEC_JPEG;
	}
	else if (!g_strcmp0(mimetype, "video/x-vp8")) {
		return MFX_CODEC_VP8;
	}

	return 0;
}
