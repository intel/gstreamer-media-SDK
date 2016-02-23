#ifndef GST_MFX_PRIME_BUFFER_PROXY_PRIV_H
#define GST_MFX_PRIME_BUFFER_PROXY_PRIV_H

#include "gstmfxprimebufferproxy.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

/**
* GST_MFX_PRIME_BUFFER_PROXY_HANDLE:
* @buf: a #GstVaapiBufferProxy
*
* Macro that evaluates to the handle of the underlying VA buffer @buf
*/
#undef  GST_MFX_PRIME_BUFFER_PROXY_HANDLE
#define GST_MFX_PRIME_BUFFER_PROXY_HANDLE(buf) \
	(GST_MFX_PRIME_BUFFER_PROXY (buf)->fd)

#define GST_MFX_PRIME_BUFFER_PROXY_VAAPI_IMAGE(buf) \
	(GST_MFX_PRIME_BUFFER_PROXY (buf)->image)

#define GST_MFX_PRIME_BUFFER_PROXY_VAIMAGE(buf) \
	(GST_MFX_PRIME_BUFFER_PROXY_VAAPI_IMAGE(buf)->image)


struct _GstMfxPrimeBufferProxy {
	/*< private >*/
    GstMfxMiniObject	parent_instance;
    GstMfxSurfaceProxy *parent;
    GstVaapiImage      *image;
    VABufferInfo        buf_info;
    VAImage            *va_img;
    guintptr            fd;
};

GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_new_from_surface(GstMfxSurfaceProxy * parent);

#define gst_mfx_prime_buffer_proxy_ref_internal(proxy) \
	((gpointer) gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (proxy)))

#define gst_mfx_prime_buffer_proxy_unref_internal(proxy) \
	gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (proxy))

#define gst_mfx_prime_buffer_proxy_replace_internal(old_proxy_ptr, new_proxy) \
	gst_mfx_mini_object_replace ((GstMfxMiniObject **)(old_proxy_ptr), \
	GST_MFX_MINI_OBJECT (new_proxy))

#undef  gst_mfx_prime_buffer_proxy_ref
#define gst_mfx_prime_buffer_proxy_ref(proxy) \
	gst_mfx_prime_buffer_proxy_ref_internal ((proxy))

#undef  gst_mfx_prime_buffer_proxy_unref
#define gst_mfx_prime_buffer_proxy_unref(proxy) \
	gst_mfx_prime_buffer_proxy_unref_internal ((proxy))

#undef  gst_mfx_prime_buffer_proxy_replace
#define gst_mfx_prime_buffer_proxy_replace(old_proxy_ptr, new_proxy) \
	gst_mfx_prime_buffer_proxy_replace_internal ((old_proxy_ptr), (new_proxy))

G_END_DECLS

#endif /* GST_MFX_PRIME_BUFFER_PROXY_PRIV_H */
