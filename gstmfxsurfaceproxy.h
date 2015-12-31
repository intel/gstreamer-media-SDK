#ifndef GST_MFX_SURFACE_PROXY_H
#define GST_MFX_SURFACE_PROXY_H

#include "gstmfxsurface.h"
#include "gstmfxsurfacepool.h"

G_BEGIN_DECLS

/**
* GST_MFX_SURFACE_PROXY_SURFACE:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the #GstMfxSurface of @proxy.
*/
#define GST_MFX_SURFACE_PROXY_SURFACE(proxy) \
	gst_mfx_surface_proxy_get_surface (proxy)

/**
* GST_MFX_SURFACE_PROXY_TIMESTAMP:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the presentation timestamp of the
* underlying @proxy surface.
*/
#define GST_MFX_SURFACE_PROXY_TIMESTAMP(proxy) \
	gst_mfx_surface_proxy_get_timestamp (proxy)

/**
* GST_MFX_SURFACE_PROXY_DURATION:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the presentation duration of the
* underlying @proxy surface.
*/
#define GST_MFX_SURFACE_PROXY_DURATION(proxy) \
	gst_mfx_surface_proxy_get_duration (proxy)

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new(GstMfxSurface * surface);

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

GstMfxSurface *
gst_mfx_surface_proxy_get_surface(GstMfxSurfaceProxy * proxy);

GstMfxID
gst_mfx_surface_proxy_get_surface_id(GstMfxSurfaceProxy * proxy);

mfxFrameSurface1 *
gst_mfx_surface_proxy_get_frame_surface(GstMfxSurfaceProxy * surface);

GstClockTime
gst_mfx_surface_proxy_get_timestamp(GstMfxSurfaceProxy * proxy);

GstClockTime
gst_mfx_surface_proxy_get_duration(GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_set_destroy_notify(GstMfxSurfaceProxy * proxy,
	GDestroyNotify destroy_func, gpointer user_data);

const GstMfxRectangle *
gst_mfx_surface_proxy_get_crop_rect(GstMfxSurfaceProxy * proxy);

void
gst_mfx_surface_proxy_set_crop_rect(GstMfxSurfaceProxy * proxy,
	const GstMfxRectangle * crop_rect);

G_END_DECLS

#endif /* GST_MFX_SURFACE_PROXY_H */
