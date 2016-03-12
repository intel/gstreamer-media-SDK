#include "video-utils.h"

/* Check VA status for success or print out an error */
gboolean
vaapi_check_status (VAStatus status, const gchar * msg)
{
    if (status != VA_STATUS_SUCCESS) {
        GST_DEBUG ("%s: %s", msg, vaErrorStr (status));
        return FALSE;
    }
    return TRUE;
}

GstVideoFormat
gst_video_format_from_mfx_fourcc(mfxU32 fourcc)
{
	switch (fourcc) {
	case MFX_FOURCC_YUY2:
		return GST_VIDEO_FORMAT_YUY2;
	case MFX_FOURCC_RGB4:
		return GST_VIDEO_FORMAT_RGBA;
	case MFX_FOURCC_YV12:
		return GST_VIDEO_FORMAT_YV12;
	case MFX_FOURCC_NV12:
		return GST_VIDEO_FORMAT_NV12;
	default:
		return GST_VIDEO_FORMAT_UNKNOWN;
	};
}

mfxU32
gst_video_format_to_mfx_fourcc(GstVideoFormat format)
{
	switch (format) {
	case GST_VIDEO_FORMAT_YUY2:
		return MFX_FOURCC_YUY2;
	case GST_VIDEO_FORMAT_RGBA:
		return MFX_FOURCC_RGB4;
	case GST_VIDEO_FORMAT_YV12:
		return MFX_FOURCC_YV12;
	case GST_VIDEO_FORMAT_NV12:
		return MFX_FOURCC_NV12;
	default:
		return 0;
	};
}
