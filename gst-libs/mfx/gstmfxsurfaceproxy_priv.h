#ifndef GST_MFX_SURFACE_PROXY_PRIV_H
#define GST_MFX_SURFACE_PROXY_PRIV_H

#include "gstmfxcontext.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

struct _GstMfxSurfaceProxy
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxContext *ctx;
	GstMfxSurfacePool *pool;

	mfxFrameSurface1 *surface;
	GstVideoFormat format;
	guint width;
	guint height;
	GstMfxRectangle crop_rect;
};

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_internal(GstMfxContext * ctx);

#define gst_mfx_surface_proxy_ref_internal(proxy) \
	((gpointer) gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (proxy)))

#define gst_mfx_surface_proxy_unref_internal(proxy) \
	gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (proxy))

#define gst_mfx_surface_proxy_replace_internal(old_proxy_ptr, new_proxy) \
	gst_mfx_mini_object_replace ((GstMfxMiniObject **)(old_proxy_ptr), \
	GST_MFX_MINI_OBJECT (new_proxy))

#undef  gst_mfx_surface_proxy_ref
#define gst_mfx_surface_proxy_ref(proxy) \
	gst_mfx_surface_proxy_ref_internal ((proxy))

#undef  gst_mfx_surface_proxy_unref
#define gst_mfx_surface_proxy_unref(proxy) \
	gst_mfx_surface_proxy_unref_internal ((proxy))

#undef  gst_mfx_surface_proxy_replace
#define gst_mfx_surface_proxy_replace(old_proxy_ptr, new_proxy) \
	gst_mfx_surface_proxy_replace_internal ((old_proxy_ptr), (new_proxy))

G_END_DECLS

#endif /* GST_MFX_SURFACE_PROXY_PRIV_H */
