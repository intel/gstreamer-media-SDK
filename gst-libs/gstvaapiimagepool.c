#include "gstvaapiimagepool.h"
#include "gstmfxobjectpool_priv.h"

/**
 * GstVaapiImagePool:
 *
 * A pool of lazily allocated #GstVaapiImage objects.
 */
struct _GstVaapiImagePool
{
    /*< private >*/
    GstMfxObjectPool parent_instance;

    GstVideoFormat format;
    guint width;
    guint height;
};

static gboolean
image_pool_init (GstMfxObjectPool * base_pool, const GstVideoInfo * vip)
{
    GstVaapiImagePool *const pool = GST_VAAPI_IMAGE_POOL (base_pool);

    pool->format = GST_VIDEO_INFO_FORMAT (vip);
    pool->width = GST_VIDEO_INFO_WIDTH (vip);
    pool->height = GST_VIDEO_INFO_HEIGHT (vip);

    return TRUE;
}

static gpointer
gst_vaapi_image_pool_alloc_object (GstMfxObjectPool * base_pool)
{
    GstVaapiImagePool *const pool = GST_VAAPI_IMAGE_POOL (base_pool);

	return gst_vaapi_image_new(base_pool->display,
        pool->width, pool->height);
}

static inline const GstMfxMiniObjectClass *
gst_vaapi_image_pool_class (void)
{
    static const GstMfxObjectPoolClass GstVaapiImagePoolClass = {
        {sizeof (GstVaapiImagePool),
        (GDestroyNotify) gst_mfx_object_pool_finalize},
        .alloc_object = gst_vaapi_image_pool_alloc_object
    };
    return GST_MFX_MINI_OBJECT_CLASS (&GstVaapiImagePoolClass);
}

GstMfxObjectPool *
gst_vaapi_image_pool_new(GstMfxDisplay * display, const GstVideoInfo * vip)
{
    GstMfxObjectPool *pool;

    g_return_val_if_fail (display != NULL, NULL);
    g_return_val_if_fail (vip != NULL, NULL);

    pool = (GstMfxObjectPool *)
      gst_mfx_mini_object_new (gst_vaapi_image_pool_class ());
    if (!pool)
        return NULL;

    gst_mfx_object_pool_init (pool, display,
        GST_MFX_POOL_OBJECT_TYPE_IMAGE);

    if (!image_pool_init (pool, vip))
        goto error;
    return pool;

error:
    gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (pool));
    return NULL;
}
