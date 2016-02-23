#include "sysdeps.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxobjectpool_priv.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfacePool
{
    GstMfxObjectPool parent_instance;

	GstMfxContextAllocator *alloc_ctx;
};

static void
surface_pool_init(GstMfxSurfacePool * pool,
	GstMfxContextAllocator * ctx)
{
	pool->alloc_ctx = ctx;
}

static void
surface_pool_finalize(GstMfxSurfacePool * pool)
{
	pool->alloc_ctx = NULL;
}


static gpointer
gst_mfx_surface_pool_alloc_object(GstMfxObjectPool * base_pool)
{
    GstMfxSurfacePool *const pool = GST_MFX_SURFACE_POOL (base_pool);

	return gst_mfx_surface_proxy_new(pool->alloc_ctx);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_pool_class (void)
{
    static const GstMfxObjectPoolClass GstMfxSurfacePoolClass = {
        { sizeof (GstMfxSurfacePool),
		(GDestroyNotify) surface_pool_finalize },
        .alloc_object = gst_mfx_surface_pool_alloc_object
    };
    return GST_MFX_MINI_OBJECT_CLASS (&GstMfxSurfacePoolClass);
}

GstMfxObjectPool *
gst_mfx_surface_pool_new(GstMfxContextAllocator * ctx)
{
	GstMfxObjectPool *pool;

	g_return_val_if_fail(ctx != NULL, NULL);

	pool = (GstMfxObjectPool *)
		gst_mfx_mini_object_new(gst_mfx_surface_pool_class());
	if (!pool)
		return NULL;

    gst_mfx_object_pool_init (pool);
	surface_pool_init(GST_MFX_SURFACE_POOL(pool), ctx);

	return pool;
}
