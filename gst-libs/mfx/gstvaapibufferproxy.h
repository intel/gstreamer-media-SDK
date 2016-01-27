#ifndef GST_VAAPI_BUFFER_PROXY_H
#define GST_VAAPI_BUFFER_PROXY_H

G_BEGIN_DECLS

#define GST_VAAPI_BUFFER_PROXY(obj) \
	((GstVaapiBufferProxy *)(obj))

/**
* GST_VAAPI_BUFFER_PROXY_HANDLE:
* @buf: a #GstVaapiBufferProxy
*
* Macro that evaluates to the handle of the underlying VA buffer @buf
*/
#define GST_VAAPI_BUFFER_PROXY_HANDLE(buf) \
	gst_vaapi_buffer_proxy_get_handle (GST_VAAPI_BUFFER_PROXY (buf))

typedef struct _GstVaapiBufferProxy GstVaapiBufferProxy;

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_new();

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_ref(GstVaapiBufferProxy * proxy);

void
gst_vaapi_buffer_proxy_unref(GstVaapiBufferProxy * proxy);

void
gst_vaapi_buffer_proxy_replace(GstVaapiBufferProxy ** old_proxy_ptr,
	GstVaapiBufferProxy * new_proxy);

guintptr
gst_vaapi_buffer_proxy_get_handle(GstVaapiBufferProxy * proxy);

G_END_DECLS

#endif /* GST_VAAPI_BUFFER_PROXY_H */
