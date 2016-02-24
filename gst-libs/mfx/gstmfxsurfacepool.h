#ifndef GST_MFX_SURFACE_POOL_H
#define GST_MFX_SURFACE_POOL_H

#include "gstmfxsurfaceproxy.h"
#include "gstmfxcontext.h"
#include <glib.h>

G_BEGIN_DECLS

#define GST_MFX_SURFACE_POOL(obj) \
  ((GstMfxSurfacePool *)(obj))

GstMfxSurfacePool *
gst_mfx_surface_pool_new(GstMfxContext * ctx);

GstMfxSurfacePool *
gst_mfx_surface_pool_ref (GstMfxSurfacePool * pool);

void
gst_mfx_surface_pool_unref (GstMfxSurfacePool * pool);

void
gst_mfx_surface_pool_replace (GstMfxSurfacePool ** old_pool_ptr,
    GstMfxSurfacePool * new_pool);

GstMfxSurfaceProxy *
gst_mfx_surface_pool_get_surface (GstMfxSurfacePool * pool);

void
gst_mfx_surface_pool_put_surface (GstMfxSurfacePool * pool,
	GstMfxSurfaceProxy * surface);

guint
gst_mfx_surface_pool_get_size (GstMfxSurfacePool * pool);

guint
gst_mfx_surface_pool_get_capacity (GstMfxSurfacePool * pool);

void
gst_mfx_surface_pool_set_capacity (GstMfxSurfacePool * pool, guint capacity);

GstMfxSurfaceProxy *
gst_mfx_surface_pool_find_proxy(GstMfxSurfacePool * pool,
    mfxFrameSurface1 * surface);

G_END_DECLS

#endif /* GST_MFX_SURFACE_POOL_H */
