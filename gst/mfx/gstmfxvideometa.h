#ifndef GST_MFX_VIDEO_META_H
#define GST_MFX_VIDEO_META_H

#include <gst/video/video.h>
#include "gstmfxdisplay.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxutils_vaapi.h"

G_BEGIN_DECLS

typedef struct _GstMfxVideoMeta GstMfxVideoMeta;

#define GST_MFX_VIDEO_META_API_TYPE \
	gst_mfx_video_meta_api_get_type ()

GType
gst_mfx_video_meta_api_get_type(void);

GstMfxVideoMeta *
gst_mfx_video_meta_copy(GstMfxVideoMeta * meta);

GstMfxVideoMeta *
gst_mfx_video_meta_new(void);

GstMfxVideoMeta *
gst_mfx_video_meta_new_from_pool(GstMfxSurfacePool * pool);

GstMfxVideoMeta *
gst_mfx_video_meta_new_with_surface_proxy(GstMfxSurfaceProxy * proxy);

GstMfxVideoMeta *
gst_mfx_video_meta_ref(GstMfxVideoMeta * meta);

void
gst_mfx_video_meta_unref(GstMfxVideoMeta * meta);

void
gst_mfx_video_meta_replace (GstMfxVideoMeta ** old_meta_ptr,
    GstMfxVideoMeta * new_meta);

GstMfxSurfaceProxy *
gst_mfx_video_meta_get_surface_proxy(GstMfxVideoMeta * meta);

void
gst_mfx_video_meta_set_surface_proxy(GstMfxVideoMeta * meta,
	GstMfxSurfaceProxy * proxy);

GstMfxVideoMeta *
gst_buffer_get_mfx_video_meta(GstBuffer * buffer);

void
gst_buffer_set_mfx_video_meta(GstBuffer * buffer, GstMfxVideoMeta * meta);

G_END_DECLS

#endif  /* GST_MFX_VIDEO_META_H */
