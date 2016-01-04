#ifndef GST_MFX_VIDEO_FORMAT_H
#define GST_MFX_VIDEO_FORMAT_H

#include <gst/video/video.h>
#include <mfxvideo.h>

#include <va/va.h>

G_BEGIN_DECLS

gboolean
vaapi_check_status (VAStatus status, const gchar *msg);

const gchar *
gst_mfx_video_format_to_string(GstVideoFormat format);

GstVideoFormat
gst_mfx_video_format_from_mfx_fourcc(mfxU32 fourcc);

guint
gst_mfx_video_format_get_chroma_type(GstVideoFormat format);

G_END_DECLS

#endif /* GST_MFX_VIDEO_FORMAT_H */
