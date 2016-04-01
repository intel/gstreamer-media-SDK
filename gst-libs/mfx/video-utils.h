#ifndef GST_MFX_VIDEO_FORMAT_H
#define GST_MFX_VIDEO_FORMAT_H

#include <gst/video/video.h>
#include <mfxvideo.h>

#include <va/va.h>

G_BEGIN_DECLS

gboolean
vaapi_check_status (VAStatus status, const gchar *msg);

guint
gst_mfx_video_format_get_chroma_type(GstVideoFormat format);

G_END_DECLS

#endif /* GST_MFX_VIDEO_FORMAT_H */
