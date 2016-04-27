#include <gmodule.h>
#include <va/va_drmcommon.h>
#include "sysdeps.h"
#include "gstmfxutils_vaapi.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxprimebufferproxy.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxPrimeBufferProxy {
    /*< private >*/
    GstMfxMiniObject     parent_instance;
    GstMfxSurfaceProxy  *parent;
    VaapiImage          *image;
    VABufferInfo         buf_info;
    guintptr             fd;
    guint                data_size;
};

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
gst_mfx_prime_buffer_proxy_acquire_handle(GstMfxPrimeBufferProxy * proxy)
{
    GstMfxDisplay *display;
    VASurfaceID surf;
    VAStatus va_status;
    VAImage va_img;

    if (!proxy->parent)
	    return FALSE;

	surf = GST_MFX_SURFACE_PROXY_MEMID(proxy->parent);
	display = gst_mfx_surface_proxy_get_display(proxy->parent);
    proxy->image = gst_mfx_surface_proxy_derive_image(proxy->parent);
    vaapi_image_get_image(proxy->image, &va_img);

    if(vpg_load_symbol("vpgExtGetSurfaceHandle"))
    {
        GST_MFX_DISPLAY_LOCK(display);
        va_status = g_va_get_surface_handle(GST_MFX_DISPLAY_VADISPLAY(display),
                        &surf, &proxy->fd);
        GST_MFX_DISPLAY_UNLOCK(display);
        if(!vaapi_check_status(va_status, "vpgExtGetSurfaceHandle()"))
            return FALSE;
    } else {
        proxy->buf_info.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
        GST_MFX_DISPLAY_LOCK(display);
        va_status = vaAcquireBufferHandle(GST_MFX_DISPLAY_VADISPLAY(display),
                        va_img.buf, &proxy->buf_info);
        GST_MFX_DISPLAY_UNLOCK(display);
        if(!vaapi_check_status(va_status, "vaAcquireBufferHandle()"))
	        return FALSE;
        proxy->fd = proxy->buf_info.handle;
    }

    proxy->data_size = va_img.data_size;

    return TRUE;
}

static void
gst_mfx_prime_buffer_proxy_finalize(GstMfxPrimeBufferProxy * proxy)
{
	if(g_va_get_surface_handle)
		close(proxy->fd);
	else {
	    GstMfxDisplay *display =
            gst_mfx_surface_proxy_get_display(proxy->parent);
        VAImage va_img;

        vaapi_image_get_image(proxy->image, &va_img);

        GST_MFX_DISPLAY_LOCK(display);
	    vaReleaseBufferHandle(GST_MFX_DISPLAY_VADISPLAY(display),
            va_img.buf);
        GST_MFX_DISPLAY_UNLOCK(display);
	}

    vaapi_image_replace(&proxy->image, NULL);
	gst_mfx_surface_proxy_replace(&proxy->parent, NULL);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_prime_buffer_proxy_class(void)
{
	static const GstMfxMiniObjectClass GstMfxPrimeBufferProxyClass = {
		sizeof (GstMfxPrimeBufferProxy),
		(GDestroyNotify)gst_mfx_prime_buffer_proxy_finalize
	};
	return &GstMfxPrimeBufferProxyClass;
}

GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_new_from_surface(GstMfxSurfaceProxy * parent)
{
	GstMfxPrimeBufferProxy *proxy;

	g_return_val_if_fail(parent != NULL, NULL);

	proxy = (GstMfxPrimeBufferProxy *)
		gst_mfx_mini_object_new0(gst_mfx_prime_buffer_proxy_class());
	if (!proxy)
		return NULL;

	proxy->parent = gst_mfx_surface_proxy_ref(parent);

	if (!gst_mfx_prime_buffer_proxy_acquire_handle(proxy))
		goto error_acquire_handle;

	return proxy;
	/* ERRORS */
error_acquire_handle:
	GST_ERROR("failed to acquire the underlying PRIME buffer handle");
	gst_mfx_prime_buffer_proxy_unref(proxy);
	return NULL;
}

/**
* gst_mfx_prime_buffer_proxy_ref:
* @proxy: a #GstVaapiBufferProxy
*
* Atomically increases the reference count of the given @proxy by one.
*
* Returns: The same @proxy argument
*/
GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_ref(GstMfxPrimeBufferProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT (proxy));
}

/**
* gst_mfx_prime_buffer_proxy_unref:
* @proxy: a #GstVaapiBufferProxy
*
* Atomically decreases the reference count of the @proxy by one. If
* the reference count reaches zero, the object will be free'd.
*/
void
gst_mfx_prime_buffer_proxy_unref(GstMfxPrimeBufferProxy * proxy)
{
    gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT (proxy));
}

/**
* gst_mfx_prime_buffer_proxy_replace:
* @old_proxy_ptr: a pointer to a #GstMfxPrimeBufferProxy
* @new_proxy: a #GstMfxPrimeBufferProxy
*
* Atomically replaces the proxy object held in @old_proxy_ptr with
* @new_proxy. This means that @old_proxy_ptr shall reference a valid
* object. However, @new_proxy can be NULL.
*/
void
gst_mfx_prime_buffer_proxy_replace(GstMfxPrimeBufferProxy ** old_proxy_ptr,
	GstMfxPrimeBufferProxy * new_proxy)
{
	g_return_if_fail(old_proxy_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_proxy_ptr,
            GST_MFX_MINI_OBJECT(new_proxy));
}

/**
* gst_mfx_prime_buffer_proxy_get_handle:
* @proxy: a #GstMfxPrimeBufferProxy
*
* Returns the underlying VA buffer handle stored in the @proxy.
*
* Return value: the buffer handle
*/
guintptr
gst_mfx_prime_buffer_proxy_get_handle(GstMfxPrimeBufferProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return proxy->fd;
}

guint
gst_mfx_prime_buffer_proxy_get_size(GstMfxPrimeBufferProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return proxy->data_size;
}

/**
 * gst_mfx_prime_buffer_proxy_get_vaapi_image:
 * @proxy: a #GstMfxPrimeBufferProxy
 *
 * Returns the underlying VaapiImage object stored in the @proxy.
 *
 * Return value: #VaapiImage
 */
VaapiImage *
gst_mfx_prime_buffer_proxy_get_vaapi_image(GstMfxPrimeBufferProxy * proxy)
{
    g_return_val_if_fail(proxy != NULL, 0);

    return proxy->image;
}
