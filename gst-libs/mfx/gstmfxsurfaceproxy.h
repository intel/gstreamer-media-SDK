#ifndef GST_MFX_SURFACE_PROXY_H
#define GST_MFX_SURFACE_PROXY_H

#include <gst/video/video.h>
#include "gstmfxtask.h"
#include "gstmfxutils_vaapi.h"
#include "video-format.h"

G_BEGIN_DECLS

#define GST_MFX_SURFACE_PROXY(obj) \
	((GstMfxSurfaceProxy *) (obj))

#define GST_MFX_SURFACE_PROXY_SURFACE(proxy) \
	gst_mfx_surface_proxy_get_frame_surface(proxy)

#define GST_MFX_SURFACE_PROXY_MEMID(proxy) \
	gst_mfx_surface_proxy_get_id(proxy)

#define GST_MFX_SURFACE_PROXY_FORMAT(proxy) \
	gst_mfx_surface_proxy_get_format(proxy)

#define GST_MFX_SURFACE_PROXY_WIDTH(proxy) \
	gst_mfx_surface_proxy_get_width(proxy)

#define GST_MFX_SURFACE_PROXY_HEIGHT(proxy) \
	gst_mfx_surface_proxy_get_height(proxy)

typedef struct _GstMfxSurfacePool GstMfxSurfacePool;
typedef struct _GstMfxSurfaceProxy GstMfxSurfaceProxy;

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new(GstMfxTask * ctx);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_from_pool(GstMfxSurfacePool * pool);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_copy(GstMfxSurfaceProxy * proxy);

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_ref(GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_unref(GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_replace(GstMfxSurfaceProxy ** old_proxy_ptr,
	GstMfxSurfaceProxy * new_proxy);

mfxFrameSurface1 *
gst_mfx_surface_proxy_get_frame_surface(GstMfxSurfaceProxy * proxy);

GstMfxID
gst_mfx_surface_proxy_get_id(GstMfxSurfaceProxy * proxy);

GstVideoFormat
gst_mfx_surface_proxy_get_format(GstMfxSurfaceProxy * proxy);

guint
gst_mfx_surface_proxy_get_width(GstMfxSurfaceProxy * proxy);

guint
gst_mfx_surface_proxy_get_height(GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_get_size(GstMfxSurfaceProxy * proxy, guint * width_ptr,
	guint * height_ptr);

GstMfxTask *
gst_mfx_surface_proxy_get_task_context(GstMfxSurfaceProxy * proxy);

const GstMfxRectangle *
gst_mfx_surface_proxy_get_crop_rect(GstMfxSurfaceProxy * proxy);

VaapiImage *
gst_mfx_surface_proxy_derive_image(GstMfxSurfaceProxy * proxy);

G_END_DECLS

#endif /* GST_MFX_SURFACE_PROXY_H */
