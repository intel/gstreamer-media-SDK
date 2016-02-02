#include "video-format.h"

GstVideoFormat
gst_mfx_video_format_from_mfx_fourcc(mfxU32 fourcc)
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