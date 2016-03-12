#include "sysdeps.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxminiobject.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfacePool
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxTask *task;
	GQueue free_objects;
	GList *used_objects;
	guint used_count;
	guint capacity;
	GMutex mutex;
};

static gint
sync_output_surface(gconstpointer proxy, gconstpointer surf)
{
    GstMfxSurfaceProxy *_proxy = (GstMfxSurfaceProxy *)proxy;
    mfxFrameSurface1 *_surf = (mfxFrameSurface1 *)surf;

    return (*(GstMfxID *)(_surf->Data.MemId) !=
            GST_MFX_SURFACE_PROXY_MEMID(_proxy));
}

void
gst_mfx_surface_pool_init (GstMfxSurfacePool * pool, GstMfxTask * task)
{
	pool->task = gst_mfx_task_ref(task);

    pool->used_objects = NULL;
    pool->used_count = 0;
    pool->capacity = 0;

    g_queue_init (&pool->free_objects);
    g_mutex_init (&pool->mutex);
}

void
gst_mfx_surface_pool_finalize (GstMfxSurfacePool * pool)
{
	gst_mfx_task_unref(pool->task);
    g_list_free_full (pool->used_objects, gst_mfx_mini_object_unref);
    g_queue_foreach (&pool->free_objects, (GFunc) gst_mfx_mini_object_unref, NULL);
    g_queue_clear (&pool->free_objects);
    g_mutex_clear (&pool->mutex);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_pool_class(void)
{
	static const GstMfxMiniObjectClass GstMfxSurfacePoolClass = {
		sizeof(GstMfxSurfacePool),
		(GDestroyNotify)gst_mfx_surface_pool_finalize
	};
	return &GstMfxSurfacePoolClass;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_new(GstMfxTask * ctx)
{
	GstMfxSurfacePool *pool;

	g_return_val_if_fail(ctx != NULL, NULL);

	pool = (GstMfxSurfacePool *)
		gst_mfx_mini_object_new(gst_mfx_surface_pool_class());
	if (!pool)
		return NULL;

	gst_mfx_surface_pool_init(pool, ctx);

	return pool;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_ref (GstMfxSurfacePool * pool)
{
	g_return_val_if_fail(pool != NULL, NULL);

	return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(pool));
}

void
gst_mfx_surface_pool_unref (GstMfxSurfacePool * pool)
{
	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(pool));
}

void
gst_mfx_surface_pool_replace (GstMfxSurfacePool ** old_pool_ptr,
    GstMfxSurfacePool * new_pool)
{
	g_return_if_fail(old_pool_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_pool_ptr,
		GST_MFX_MINI_OBJECT(new_pool));
}

static void
release_surfaces(gpointer proxy, gpointer pool)
{
	GstMfxSurfaceProxy *_proxy = (GstMfxSurfaceProxy *)proxy;
	GstMfxSurfacePool *_pool = (GstMfxSurfacePool *)pool;

	mfxFrameSurface1 *surface = gst_mfx_surface_proxy_get_frame_surface(_proxy);
	if (surface && !surface->Data.Locked)
		gst_mfx_surface_pool_put_surface(_pool, _proxy);
}

static GstMfxSurfaceProxy *
gst_mfx_surface_pool_get_surface_unlocked (GstMfxSurfacePool * pool)
{
	GstMfxSurfaceProxy *surface;

    if (pool->capacity && pool->used_count >= pool->capacity)
        return NULL;

    surface = g_queue_pop_head (&pool->free_objects);

    if (!surface) {
        g_mutex_unlock (&pool->mutex);
        surface = gst_mfx_surface_proxy_new(pool->task);
        g_mutex_lock (&pool->mutex);
    if (!surface)
        return NULL;
    }

    ++pool->used_count;
    pool->used_objects = g_list_prepend (pool->used_objects, surface);
    return gst_mfx_surface_proxy_ref (surface);
}

GstMfxSurfaceProxy *
gst_mfx_surface_pool_get_surface (GstMfxSurfacePool * pool)
{
	GstMfxSurfaceProxy *surface;

    g_return_val_if_fail (pool != NULL, NULL);

    g_list_foreach(pool->used_objects, release_surfaces, pool);

    g_mutex_lock (&pool->mutex);
    surface = gst_mfx_surface_pool_get_surface_unlocked (pool);
    g_mutex_unlock (&pool->mutex);
    return surface;
}

static void
gst_mfx_surface_pool_put_surface_unlocked (GstMfxSurfacePool * pool,
    GstMfxSurfaceProxy * surface)
{
    GList *elem;

    elem = g_list_find (pool->used_objects, surface);
    if (!elem)
        return;

    gst_mfx_surface_proxy_unref (surface);
    --pool->used_count;
    pool->used_objects = g_list_delete_link (pool->used_objects, elem);
    g_queue_push_tail (&pool->free_objects, surface);
}

void
gst_mfx_surface_pool_put_surface (GstMfxSurfacePool * pool,
	GstMfxSurfaceProxy * surface)
{
    g_return_if_fail (pool != NULL);
    g_return_if_fail (surface != NULL);

    g_mutex_lock (&pool->mutex);
    gst_mfx_surface_pool_put_surface_unlocked (pool, surface);
    g_mutex_unlock (&pool->mutex);
}

guint
gst_mfx_surface_pool_get_size (GstMfxSurfacePool * pool)
{
    guint size;

    g_return_val_if_fail (pool != NULL, 0);

    g_mutex_lock (&pool->mutex);
    size = g_queue_get_length (&pool->free_objects);
    g_mutex_unlock (&pool->mutex);
    return size;
}

guint
gst_mfx_surface_pool_get_capacity (GstMfxSurfacePool * pool)
{
    guint capacity;

    g_return_val_if_fail (pool != NULL, 0);

    g_mutex_lock (&pool->mutex);
    capacity = pool->capacity;
    g_mutex_unlock (&pool->mutex);

    return capacity;
}

void
gst_mfx_surface_pool_set_capacity (GstMfxSurfacePool * pool, guint capacity)
{
    g_return_if_fail (pool != NULL);

    g_mutex_lock (&pool->mutex);
    pool->capacity = capacity;
    g_mutex_unlock (&pool->mutex);
}

GstMfxSurfaceProxy *
gst_mfx_surface_pool_find_proxy(GstMfxSurfacePool * pool,
    mfxFrameSurface1 * surface)
{
    g_return_val_if_fail(pool != NULL, NULL);

    GList *l = g_list_find_custom(pool->used_objects, surface,
                    sync_output_surface);

    return GST_MFX_SURFACE_PROXY(l->data);
}
