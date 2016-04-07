#ifndef GST_MFX_VIDEO_FORMAT_H
#define GST_MFX_VIDEO_FORMAT_H

#include <gst/video/video.h>
#include <mfxvideo.h>

#include <va/va.h>

G_BEGIN_DECLS

gboolean
vaapi_check_status (VAStatus status, const gchar *msg);

mfxStatus
vaapi_to_mfx_status(VAStatus va_sts);

G_END_DECLS

#endif /* GST_MFX_VIDEO_FORMAT_H */
