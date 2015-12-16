#ifndef GST_MFX_SURFACE_PROXY_PRIV_H
#define GST_MFX_SURFACE_PROXY_PRIV_H

#include "gstmfxminiobject.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurface_priv.h"

#define GST_MFX_SURFACE_PROXY(obj) \
	((GstMfxSurfaceProxy *) (obj))

struct _GstMfxSurfaceProxy
{
	/*< private >*/
	GstMfxMiniObject parent_instance;
	GstMfxSurfaceProxy *parent;

	GstMfxSurfacePool *pool;
	GstMfxSurface *surface;
	guintptr view_id;
	GstClockTime timestamp;
	GstClockTime duration;
	GDestroyNotify destroy_func;
	gpointer destroy_data;
	GstMfxRectangle crop_rect;
	guint has_crop_rect : 1;
};

#define GST_MFX_SURFACE_PROXY_FLAGS       GST_MFX_MINI_OBJECT_FLAGS
#define GST_MFX_SURFACE_PROXY_FLAG_IS_SET GST_MFX_MINI_OBJECT_FLAG_IS_SET
#define GST_MFX_SURFACE_PROXY_FLAG_SET    GST_MFX_MINI_OBJECT_FLAG_SET
#define GST_MFX_SURFACE_PROXY_FLAG_UNSET  GST_MFX_MINI_OBJECT_FLAG_UNSET

/**
* GST_MFX_SURFACE_PROXY_SURFACE:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the #GstMfxSurface of @proxy.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_PROXY_SURFACE
#define GST_MFX_SURFACE_PROXY_SURFACE(proxy) \
	(GST_MFX_SURFACE_PROXY (proxy)->surface)

/**
* GST_MFX_SURFACE_PROXY_TIMESTAMP:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the presentation timestamp of the
* underlying @proxy surface.
*
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_PROXY_TIMESTAMP
#define GST_MFX_SURFACE_PROXY_TIMESTAMP(proxy) \
	(GST_MFX_SURFACE_PROXY (proxy)->timestamp)

/**
* GST_MFX_SURFACE_PROXY_DURATION:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the presentation duration of the
* underlying @proxy surface.
*
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_PROXY_DURATION
#define GST_MFX_SURFACE_PROXY_DURATION(proxy) \
	(GST_MFX_SURFACE_PROXY (proxy)->duration)

/**
* GST_MFX_SURFACE_PROXY_CROP_RECT:
* @proxy: a #GstMfxSurfaceProxy
*
* Macro that evaluates to the video cropping rectangle of the underlying @proxy surface.
*
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_PROXY_CROP_RECT
#define GST_MFX_SURFACE_PROXY_CROP_RECT(proxy) \
	(GST_MFX_SURFACE_PROXY (proxy)->has_crop_rect ? \
	&GST_MFX_SURFACE_PROXY (proxy)->crop_rect : NULL)

#endif /* GST_MFX_SURFACE_PROXY_PRIV_H */