#include "sysdeps.h"
#include "gstvaapibufferproxy.h"
#include "gstvaapibufferproxy_priv.h"
#include "video-utils.h"
#include "gstmfxobject_priv.h"
#include <gmodule.h>


/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_vaapi_buffer_proxy_ref
#undef gst_vaapi_buffer_proxy_unref
#undef gst_vaapi_buffer_proxy_replace

typedef VAStatus(*vaExtGetSurfaceHandle)(
	VADisplay dpy,
	VASurfaceID *surface,
	int *prime_fd);

static vaExtGetSurfaceHandle g_va_get_surface_handle;

static gboolean
vpg_load_symbol(const gchar* vpg_extension)
{
	GModule *module;

	if (g_va_get_surface_handle)
		return TRUE;

	module = g_module_open("iHD_drv_video.so", G_MODULE_BIND_LAZY |
        G_MODULE_BIND_LOCAL);
	if (!module)
		return FALSE;

	g_module_symbol(module, vpg_extension, (gpointer *)&g_va_get_surface_handle);
	if (!g_va_get_surface_handle) {
		g_module_close(module);
		return FALSE;
	}

	return TRUE;
}

static gboolean
gst_vaapi_buffer_proxy_acquire_handle(GstVaapiBufferProxy * proxy)
{
    VASurfaceID surf = GST_MFX_OBJECT_ID(proxy->parent);
	VAStatus va_status;

	if (!proxy->parent)
		return FALSE;

    if(!vpg_load_symbol("vpgExtGetSurfaceHandle"))
        return FALSE;

	GST_MFX_OBJECT_LOCK_DISPLAY(proxy->parent);
	va_status = g_va_get_surface_handle(GST_MFX_OBJECT_VADISPLAY(proxy->parent),
		&surf, &proxy->fd);
	GST_MFX_OBJECT_UNLOCK_DISPLAY(proxy->parent);
	if (!vaapi_check_status(va_status, "vaExtGetSurfaceHandle()"))
		return FALSE;

	return TRUE;
}

static void
gst_vaapi_buffer_proxy_finalize(GstVaapiBufferProxy * proxy)
{
	gst_mfx_object_replace(&proxy->parent, NULL);
	close(proxy->fd);
}

static inline const GstMfxMiniObjectClass *
gst_vaapi_buffer_proxy_class(void)
{
	static const GstMfxMiniObjectClass GstVaapiBufferProxyClass = {
		sizeof (GstVaapiBufferProxy),
		(GDestroyNotify)gst_vaapi_buffer_proxy_finalize
	};
	return &GstVaapiBufferProxyClass;
}

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_new()
{
	GstVaapiBufferProxy *proxy;

	proxy = (GstVaapiBufferProxy *)
		gst_mfx_mini_object_new(gst_vaapi_buffer_proxy_class());
	if (!proxy)
		return NULL;

	proxy->parent = NULL;

	return proxy;
}

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_new_from_object(GstMfxObject * object)
{
	GstVaapiBufferProxy *proxy;

	g_return_val_if_fail(object != NULL, NULL);

	proxy = (GstVaapiBufferProxy *)
		gst_mfx_mini_object_new(gst_vaapi_buffer_proxy_class());
	if (!proxy)
		return NULL;

	proxy->parent = gst_mfx_object_ref(object);

	if (!gst_vaapi_buffer_proxy_acquire_handle(proxy))
		goto error_acquire_handle;
	return proxy;

	/* ERRORS */
error_acquire_handle:
	GST_ERROR("failed to acquire the underlying VA buffer handle");
	gst_vaapi_buffer_proxy_unref_internal(proxy);
	return NULL;
}

/**
* gst_vaapi_buffer_proxy_ref:
* @proxy: a #GstVaapiBufferProxy
*
* Atomically increases the reference count of the given @proxy by one.
*
* Returns: The same @proxy argument
*/
GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_ref(GstVaapiBufferProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return gst_vaapi_buffer_proxy_ref_internal(proxy);
}

/**
* gst_vaapi_buffer_proxy_unref:
* @proxy: a #GstVaapiBufferProxy
*
* Atomically decreases the reference count of the @proxy by one. If
* the reference count reaches zero, the object will be free'd.
*/
void
gst_vaapi_buffer_proxy_unref(GstVaapiBufferProxy * proxy)
{
	g_return_if_fail(proxy != NULL);

	gst_vaapi_buffer_proxy_unref_internal(proxy);
}

/**
* gst_vaapi_buffer_proxy_replace:
* @old_proxy_ptr: a pointer to a #GstVaapiBufferProxy
* @new_proxy: a #GstVaapiBufferProxy
*
* Atomically replaces the proxy object held in @old_proxy_ptr with
* @new_proxy. This means that @old_proxy_ptr shall reference a valid
* object. However, @new_proxy can be NULL.
*/
void
gst_vaapi_buffer_proxy_replace(GstVaapiBufferProxy ** old_proxy_ptr,
	GstVaapiBufferProxy * new_proxy)
{
	g_return_if_fail(old_proxy_ptr != NULL);

	gst_vaapi_buffer_proxy_replace_internal(old_proxy_ptr, new_proxy);
}

/**
* gst_vaapi_buffer_proxy_get_handle:
* @proxy: a #GstVaapiBufferProxy
*
* Returns the underlying VA buffer handle stored in the @proxy.
*
* Return value: the buffer handle
*/
guintptr
gst_vaapi_buffer_proxy_get_handle(GstVaapiBufferProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return GST_VAAPI_BUFFER_PROXY_HANDLE(proxy);
}
