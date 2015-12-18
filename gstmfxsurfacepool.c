#include "gstmfxsurfacepool.h"
#include "gstmfxobjectpool_priv.h"

/**
* GstMfxSurfacePool:
*
* A pool of lazily allocated #GstMfxSurface objects.
*/

struct _GstMfxSurfacePool
{
    GstMfxObjectPool parent_instance;

	//GstVideoInfo video_info;
	GstMfxContextAllocatorVaapi *ctx;
};

static gboolean
surface_pool_init(GstMfxSurfacePool * pool, GstMfxContextAllocatorVaapi * ctx)
{
	/*GstVideoFormat format = gst_mfx_video_format_from_mfx_fourcc(ctx->frame_info.FourCC);

	if (format == GST_VIDEO_FORMAT_UNKNOWN)
		return FALSE;

	gst_video_info_set_format(&(pool->video_info), format,
        ctx->frame_info.Width, ctx->frame_info.Height);*/

	pool->ctx = ctx;

	return TRUE;
}

static gpointer
gst_mfx_surface_pool_alloc_object(GstMfxObjectPool * base_pool)
{
    GstMfxSurfacePool *const pool = GST_MFX_SURFACE_POOL (base_pool);

	return gst_mfx_surface_new(pool->ctx);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_pool_class (void)
{
    static const GstMfxObjectPoolClass GstMfxSurfacePoolClass = {
        {sizeof (GstMfxSurfacePool),
        (GDestroyNotify) gst_mfx_object_pool_finalize},
        .alloc_object = gst_mfx_surface_pool_alloc_object
    };
    return GST_MFX_MINI_OBJECT_CLASS (&GstMfxSurfacePoolClass);
}

GstMfxObjectPool *
gst_mfx_surface_pool_new(GstMfxContextAllocatorVaapi * ctx)
{
	GstMfxObjectPool *pool;

	g_return_val_if_fail(ctx != NULL, NULL);

	pool = (GstMfxObjectPool *)
		gst_mfx_mini_object_new(gst_mfx_surface_pool_class());
	if (!pool)
		return NULL;

    gst_mfx_object_pool_init (pool, //display,
        GST_MFX_POOL_OBJECT_TYPE_SURFACE);
	if (!surface_pool_init(GST_MFX_SURFACE_POOL(pool), ctx))
		goto error;
	return pool;

error:
	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(pool));
	return NULL;
}
