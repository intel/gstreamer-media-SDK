#ifndef GST_VAAPI_BUFFER_PROXY_PRIV_H
#define GST_VAAPI_BUFFER_PROXY_PRIV_H

#include "gstvaapibufferproxy.h"
#include "gstmfxsurface.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

/**
* GST_VAAPI_BUFFER_PROXY_HANDLE:
* @buf: a #GstVaapiBufferProxy
*
* Macro that evaluates to the handle of the underlying VA buffer @buf
*/
#undef  GST_VAAPI_BUFFER_PROXY_HANDLE
#define GST_VAAPI_BUFFER_PROXY_HANDLE(buf) \
	(GST_VAAPI_BUFFER_PROXY (buf)->fd)


struct _GstVaapiBufferProxy {
	/*< private >*/
	GstMfxMiniObject	parent_instance;
	GstMfxObject       *parent;

	int					fd;
};

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_new_from_object(GstMfxObject * object);

#define gst_vaapi_buffer_proxy_ref_internal(proxy) \
	((gpointer) gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (proxy)))

#define gst_vaapi_buffer_proxy_unref_internal(proxy) \
	gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (proxy))

#define gst_vaapi_buffer_proxy_replace_internal(old_proxy_ptr, new_proxy) \
	gst_mfx_mini_object_replace ((GstMfxMiniObject **)(old_proxy_ptr), \
	GST_MFX_MINI_OBJECT (new_proxy))

#undef  gst_vaapi_buffer_proxy_ref
#define gst_vaapi_buffer_proxy_ref(proxy) \
	gst_vaapi_buffer_proxy_ref_internal ((proxy))

#undef  gst_vaapi_buffer_proxy_unref
#define gst_vaapi_buffer_proxy_unref(proxy) \
	gst_vaapi_buffer_proxy_unref_internal ((proxy))

#undef  gst_vaapi_buffer_proxy_replace
#define gst_vaapi_buffer_proxy_replace(old_proxy_ptr, new_proxy) \
	gst_vaapi_buffer_proxy_replace_internal ((old_proxy_ptr), (new_proxy))

G_END_DECLS

#endif /* GST_VAAPI_BUFFER_PROXY_PRIV_H */
