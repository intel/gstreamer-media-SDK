#ifndef GST_MFX_VIDEO_FORMAT_H
#define GST_MFX_VIDEO_FORMAT_H

#include <gst/video/video.h>
#include <mfxvideo.h>

#include <va/va.h>

G_BEGIN_DECLS

const gchar *
gst_mfx_video_format_to_string(GstVideoFormat format);

GstVideoFormat
gst_video_format_from_mfx_fourcc(mfxU32 fourcc);

mfxU32
gst_video_format_to_mfx_fourcc(GstVideoFormat format);

GstVideoFormat
gst_video_format_from_va_fourcc(guint fourcc);

guint
gst_video_format_to_va_fourcc(GstVideoFormat format);

mfxU32
gst_mfx_video_format_from_va_fourcc(guint fourcc);

guint
gst_mfx_video_format_to_va_fourcc(mfxU32 fourcc);

guint
gst_mfx_video_format_to_va_format(mfxU32 fourcc);

guint16
gst_mfx_chroma_type_from_video_format(GstVideoFormat format);

G_END_DECLS

#endif /* GST_MFX_VIDEO_FORMAT_H */
