#ifndef GST_VAAPI_BUFFER_PROXY_H
#define GST_VAAPI_BUFFER_PROXY_H

G_BEGIN_DECLS

#define GST_VAAPI_BUFFER_PROXY(obj) \
  ((GstVaapiBufferProxy *)(obj))

/**
 * GST_VAAPI_BUFFER_PROXY_TYPE:
 * @buf: a #GstVaapiBufferProxy
 *
 * Macro that evaluates to the type of the underlying VA buffer @buf
 */
#define GST_VAAPI_BUFFER_PROXY_TYPE(buf) \
  gst_vaapi_buffer_proxy_get_type (GST_VAAPI_BUFFER_PROXY (buf))

/**
 * GST_VAAPI_BUFFER_PROXY_HANDLE:
 * @buf: a #GstVaapiBufferProxy
 *
 * Macro that evaluates to the handle of the underlying VA buffer @buf
 */
#define GST_VAAPI_BUFFER_PROXY_HANDLE(buf) \
  gst_vaapi_buffer_proxy_get_handle (GST_VAAPI_BUFFER_PROXY (buf))

/**
 * GST_VAAPI_BUFFER_PROXY_SIZE:
 * @buf: a #GstVaapiBufferProxy
 *
 * Macro that evaluates to the size of the underlying VA buffer @buf
 */
#define GST_VAAPI_BUFFER_PROXY_SIZE(buf) \
  gst_vaapi_buffer_proxy_get_size (GST_VAAPI_BUFFER_PROXY (buf))

typedef struct _GstVaapiBufferProxy             GstVaapiBufferProxy;

/**
 * GstVaapiBufferMemoryType:
 * @GST_VAAPI_BUFFER_MEMORY_TYPE_DMA_BUF: DRM PRIME buffer memory type.
 * @GST_VAAPI_BUFFER_MEMORY_TYPE_GEM_BUF: Kernel DRM buffer memory type.
 *
 * Set of underlying VA buffer memory types.
 */
typedef enum {
  GST_VAAPI_BUFFER_MEMORY_TYPE_DMA_BUF = 1,
  GST_VAAPI_BUFFER_MEMORY_TYPE_GEM_BUF,
} GstVaapiBufferMemoryType;

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_new (guintptr handle, guint type, gsize size,
    GDestroyNotify destroy_func, gpointer user_data);

GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_ref (GstVaapiBufferProxy * proxy);

void
gst_vaapi_buffer_proxy_unref (GstVaapiBufferProxy * proxy);

void
gst_vaapi_buffer_proxy_replace (GstVaapiBufferProxy ** old_proxy_ptr,
    GstVaapiBufferProxy * new_proxy);

guint
gst_vaapi_buffer_proxy_get_type (GstVaapiBufferProxy * proxy);

guintptr
gst_vaapi_buffer_proxy_get_handle (GstVaapiBufferProxy * proxy);

gsize
gst_vaapi_buffer_proxy_get_size (GstVaapiBufferProxy * proxy);

G_END_DECLS

#endif /* GST_VAAPI_BUFFER_PROXY_H */
