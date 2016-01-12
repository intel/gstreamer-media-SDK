#ifndef GST_VAAPI_BUFFER_PROXY_PRIV_H
#define GST_VAAPI_BUFFER_PROXY_PRIV_H

#include "gstvaapibufferproxy.h"
#include "gstmfxobject.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

/**
 * GST_VAAPI_BUFFER_PROXY_TYPE:
 * @buf: a #GstVaapiBufferProxy
 *
 * Macro that evaluates to the type of the underlying VA buffer @buf
 */
#undef  GST_VAAPI_BUFFER_PROXY_TYPE
#define GST_VAAPI_BUFFER_PROXY_TYPE(buf) \
  (GST_VAAPI_BUFFER_PROXY (buf)->type)

/**
 * GST_VAAPI_BUFFER_PROXY_HANDLE:
 * @buf: a #GstVaapiBufferProxy
 *
 * Macro that evaluates to the handle of the underlying VA buffer @buf
 */
#undef  GST_VAAPI_BUFFER_PROXY_HANDLE
#define GST_VAAPI_BUFFER_PROXY_HANDLE(buf) \
  (GST_VAAPI_BUFFER_PROXY (buf)->va_info.handle)

/**
 * GST_VAAPI_BUFFER_PROXY_SIZE:
 * @buf: a #GstVaapiBufferProxy
 *
 * Macro that evaluates to the size of the underlying VA buffer @buf
 */
#undef  GST_VAAPI_BUFFER_PROXY_SIZE
#define GST_VAAPI_BUFFER_PROXY_SIZE(buf) \
  (GST_VAAPI_BUFFER_PROXY (buf)->va_info.mem_size)

struct _GstVaapiBufferProxy {
  /*< private >*/
  GstMfxMiniObject    parent_instance;
  GstMfxObject       *parent;

  GDestroyNotify        destroy_func;
  gpointer              destroy_data;
  guint                 type;
  VABufferID            va_buf;
#if VA_CHECK_VERSION (0,36,0)
  VABufferInfo          va_info;
#endif
};

G_GNUC_INTERNAL
GstVaapiBufferProxy *
gst_vaapi_buffer_proxy_new_from_object (GstMfxObject * object,
    VABufferID buf_id, guint type, GDestroyNotify destroy_func, gpointer data);

G_GNUC_INTERNAL
guint
from_GstVaapiBufferMemoryType (guint type);

G_GNUC_INTERNAL
guint
to_GstVaapiBufferMemoryType (guint va_type);

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
