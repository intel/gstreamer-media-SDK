#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurfaceproxy_priv.h"
#include "gstmfxsurfacepool.h"

#define DEBUG 1
#include "gstmfxdebug.h"

static void
gst_mfx_surface_proxy_finalize(GstMfxSurfaceProxy * proxy)
{
	if (proxy->surface) {
		if (proxy->pool && !proxy->parent)
			gst_mfx_object_pool_put_object(proxy->pool, proxy->surface);
		gst_mfx_object_unref(proxy->surface);
		proxy->surface = NULL;
	}
	gst_mfx_object_pool_replace(&proxy->pool, NULL);
	gst_mfx_surface_proxy_replace(&proxy->parent, NULL);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_proxy_class(void)
{
	static const GstMfxMiniObjectClass GstMfxSurfaceProxyClass = {
		sizeof (GstMfxSurfaceProxy),
		(GDestroyNotify)gst_mfx_surface_proxy_finalize
	};
	return &GstMfxSurfaceProxyClass;
}

static void
gst_mfx_surface_proxy_init_properties(GstMfxSurfaceProxy * proxy)
{
	proxy->timestamp = GST_CLOCK_TIME_NONE;
	proxy->duration = GST_CLOCK_TIME_NONE;
	proxy->has_crop_rect = FALSE;
}

/**
* gst_mfx_surface_proxy_new:
* @surface: a #GstMfxSurface
*
* Creates a new #GstMfxSurfaceProxy with the specified
* surface. This allows for transporting additional information that
* are not to be attached to the @surface directly.
*
* Return value: the newly allocated #GstMfxSurfaceProxy object
*/
GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new(GstMfxSurface * surface)
{
	GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(surface != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->parent = NULL;
	proxy->pool = NULL;
	proxy->surface = gst_mfx_object_ref(surface);
	if (!proxy->surface)
		goto error;
	gst_mfx_surface_proxy_init_properties(proxy);
	return proxy;

error:
	gst_mfx_surface_proxy_unref(proxy);
	return NULL;
}

/**
* gst_mfx_surface_proxy_new_from_pool:
* @pool: a #GstMfxSurfacePool
*
* Allocates a new surface from the supplied surface @pool and creates
* the wrapped surface proxy object from it. When the last reference
* to the proxy object is released, then the underlying VA surface is
* pushed back to its parent pool.
*
* Returns: The same newly allocated @proxy object, or %NULL on error
*/
GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_from_pool(GstMfxSurfacePool * pool)
{
	GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(pool != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->parent = NULL;
	proxy->pool = gst_mfx_object_pool_ref(pool);
	proxy->surface = gst_mfx_object_pool_get_object(proxy->pool);
	if (!proxy->surface)
		goto error;
	gst_mfx_object_ref(proxy->surface);
	gst_mfx_surface_proxy_init_properties(proxy);
	return proxy;

error:
	gst_mfx_surface_proxy_unref(proxy);
	return NULL;
}

/**
* gst_mfx_surface_proxy_copy:
* @proxy: the parent #GstMfxSurfaceProxy
*
* Creates are new VA surface proxy object from the supplied parent
* @proxy object with the same initial information, e.g. timestamp,
* duration.
*
* Note: the destroy notify function is not copied into the new
* surface proxy object.
*
* Returns: The same newly allocated @proxy object, or %NULL on error
*/
GstMfxSurfaceProxy *
gst_mfx_surface_proxy_copy(GstMfxSurfaceProxy * proxy)
{
	GstMfxSurfaceProxy *copy;

	g_return_val_if_fail(proxy != NULL, NULL);

	copy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new(gst_mfx_surface_proxy_class());
	if (!copy)
		return NULL;

	copy->parent = gst_mfx_surface_proxy_ref(proxy->parent ?
		proxy->parent : proxy);
	copy->pool = proxy->pool ? gst_mfx_object_pool_ref(proxy->pool) : NULL;
	copy->surface = gst_mfx_object_ref(proxy->surface);
	copy->timestamp = proxy->timestamp;
	copy->duration = proxy->duration;
	copy->has_crop_rect = proxy->has_crop_rect;
	if (copy->has_crop_rect)
		copy->crop_rect = proxy->crop_rect;
	return copy;
}

/**
* gst_mfx_surface_proxy_ref:
* @proxy: a #GstMfxSurfaceProxy
*
* Atomically increases the reference count of the given @proxy by one.
*
* Returns: The same @proxy argument
*/
GstMfxSurfaceProxy *
gst_mfx_surface_proxy_ref(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return
		GST_MFX_SURFACE_PROXY(gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT
		(proxy)));
}

/**
* gst_mfx_surface_proxy_unref:
* @proxy: a #GstMfxSurfaceProxy
*
* Atomically decreases the reference count of the @proxy by one. If
* the reference count reaches zero, the object will be free'd.
*/
void
gst_mfx_surface_proxy_unref(GstMfxSurfaceProxy * proxy)
{
	g_return_if_fail(proxy != NULL);

	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(proxy));
}

/**
* gst_mfx_surface_proxy_replace:
* @old_proxy_ptr: a pointer to a #GstMfxSurfaceProxy
* @new_proxy: a #GstMfxSurfaceProxy
*
* Atomically replaces the proxy object held in @old_proxy_ptr with
* @new_proxy. This means that @old_proxy_ptr shall reference a valid
* object. However, @new_proxy can be NULL.
*/
void
gst_mfx_surface_proxy_replace(GstMfxSurfaceProxy ** old_proxy_ptr,
	GstMfxSurfaceProxy * new_proxy)
{
	g_return_if_fail(old_proxy_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_proxy_ptr,
		GST_MFX_MINI_OBJECT(new_proxy));
}

/**
* gst_mfx_surface_proxy_get_surface:
* @proxy: a #GstMfxSurfaceProxy
*
* Returns the #GstMfxSurface stored in the @proxy.
*
* Return value: the #GstMfxSurface
*/
GstMfxSurface *
gst_mfx_surface_proxy_get_surface(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return GST_MFX_SURFACE_PROXY_SURFACE(proxy);
}

/**
* gst_mfx_surface_proxy_get_surface_id:
* @proxy: a #GstMfxSurfaceProxy
*
* Returns the VA surface ID stored in the @proxy.
*
* Return value: the #GstMfxID
*/
GstMfxID
gst_mfx_surface_proxy_get_surface_id(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, VA_INVALID_ID);
	g_return_val_if_fail(proxy->surface != NULL, VA_INVALID_ID);

	return GST_MFX_SURFACE_PROXY_SURFACE_ID(proxy);
}

mfxFrameSurface1 *
gst_mfx_surface_proxy_get_frame_surface(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return GST_MFX_SURFACE_PROXY_SURFACE(proxy)->surface;
}


/**
* gst_mfx_surface_proxy_get_timestamp:
* @proxy: a #GstMfxSurfaceProxy
*
* Returns the presentation timestamp for this surface @proxy.
*
* Return value: the presentation timestamp
*/
GstClockTime
gst_mfx_surface_proxy_get_timestamp(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return GST_MFX_SURFACE_PROXY_TIMESTAMP(proxy);
}

/**
* gst_mfx_surface_proxy_get_duration:
* @proxy: a #GstMfxSurfaceProxy
*
* Returns the presentation duration for this surface @proxy.
*
* Return value: the presentation duration
*/
GstClockTime
gst_mfx_surface_proxy_get_duration(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return GST_MFX_SURFACE_PROXY_DURATION(proxy);
}

/**
* gst_mfx_surface_proxy_get_crop_rect:
* @proxy: a #GstMfxSurfaceProxy
*
* Returns the #GstMfxRectangle stored in the @proxy and that
* represents the cropping rectangle for the underlying surface to be
* used for rendering.
*
* If no cropping rectangle was associated with the @proxy, then this
* function returns %NULL.
*
* Return value: the #GstMfxRectangle, or %NULL if none was
*   associated with the surface proxy
*/
const GstMfxRectangle *
gst_mfx_surface_proxy_get_crop_rect(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return GST_MFX_SURFACE_PROXY_CROP_RECT(proxy);
}

/**
* gst_mfx_surface_proxy_set_crop_rect:
* @proxy: #GstMfxSurfaceProxy
* @crop_rect: the #GstMfxRectangle to be stored in @proxy
*
* Associates the @crop_rect with the @proxy
*/
void
gst_mfx_surface_proxy_set_crop_rect(GstMfxSurfaceProxy * proxy,
	const GstMfxRectangle * crop_rect)
{
	g_return_if_fail(proxy != NULL);

	proxy->has_crop_rect = crop_rect != NULL;
	if (proxy->has_crop_rect)
		proxy->crop_rect = *crop_rect;
}
