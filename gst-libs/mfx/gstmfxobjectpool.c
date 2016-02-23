#include "sysdeps.h"
#include "gstmfxobjectpool.h"
#include "gstmfxobjectpool_priv.h"
#include "gstmfxminiobject.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_object_pool_ref
#undef gst_mfx_object_pool_unref
#undef gst_mfx_object_pool_replace

#define GST_MFX_OBJECT_POOL_GET_CLASS(obj) \
  gst_mfx_object_pool_get_class (GST_MFX_OBJECT_POOL (obj))

static inline const GstMfxObjectPoolClass *
gst_mfx_object_pool_get_class (GstMfxObjectPool * pool)
{
  return GST_MFX_OBJECT_POOL_CLASS (GST_MFX_MINI_OBJECT_GET_CLASS (pool));
}

static inline gpointer
gst_mfx_object_pool_alloc_object (GstMfxObjectPool * pool)
{
  return GST_MFX_OBJECT_POOL_GET_CLASS (pool)->alloc_object (pool);
}

void
gst_mfx_object_pool_init (GstMfxObjectPool * pool)
{
    pool->used_objects = NULL;
    pool->used_count = 0;
    pool->capacity = 0;

    g_queue_init (&pool->free_objects);
    g_mutex_init (&pool->mutex);
}

void
gst_mfx_object_pool_finalize (GstMfxObjectPool * pool)
{
    g_list_free_full (pool->used_objects, gst_mfx_mini_object_unref);
    g_queue_foreach (&pool->free_objects, (GFunc) gst_mfx_mini_object_unref, NULL);
    g_queue_clear (&pool->free_objects);
    g_mutex_clear (&pool->mutex);
}

/**
 * gst_mfx_object_pool_ref:
 * @pool: a #GstMfxObjectPool
 *
 * Atomically increases the reference count of the given @pool by one.
 *
 * Returns: The same @pool argument
 */
GstMfxObjectPool *
gst_mfx_object_pool_ref (GstMfxObjectPool * pool)
{
    return gst_mfx_object_pool_ref_internal (pool);
}

/**
 * gst_mfx_object_pool_unref:
 * @pool: a #GstMfxObjectPool
 *
 * Atomically decreases the reference count of the @pool by one. If
 * the reference count reaches zero, the pool will be free'd.
 */
void
gst_mfx_object_pool_unref (GstMfxObjectPool * pool)
{
    gst_mfx_object_pool_unref_internal (pool);
}

/**
 * gst_mfx_object_pool_replace:
 * @old_pool_ptr: a pointer to a #GstMfxObjectPool
 * @new_pool: a #GstMfxObjectPool
 *
 * Atomically replaces the pool pool held in @old_pool_ptr with
 * @new_pool. This means that @old_pool_ptr shall reference a valid
 * pool. However, @new_pool can be NULL.
 */
void
gst_mfx_object_pool_replace (GstMfxObjectPool ** old_pool_ptr,
    GstMfxObjectPool * new_pool)
{
    gst_mfx_object_pool_replace_internal (old_pool_ptr, new_pool);
}

/**
 * gst_mfx_object_pool_get_object:
 * @pool: a #GstMfxObjectPool
 *
 * Retrieves a new object from the @pool, or allocates a new one if
 * none was found. The @pool holds a reference on the returned object
 * and thus shall be released through gst_mfx_object_pool_put_object()
 * when it's no longer needed.
 *
 * Return value: a possibly newly allocated object, or %NULL on error
 */
static gpointer
gst_mfx_object_pool_get_object_unlocked (GstMfxObjectPool * pool)
{
    gpointer object;

    if (pool->capacity && pool->used_count >= pool->capacity)
        return NULL;

    object = g_queue_pop_head (&pool->free_objects);

    if (!object) {
        g_mutex_unlock (&pool->mutex);
        object = gst_mfx_object_pool_alloc_object (pool);
        g_mutex_lock (&pool->mutex);
    if (!object)
        return NULL;
    }

    ++pool->used_count;
    pool->used_objects = g_list_prepend (pool->used_objects, object);
    return gst_mfx_mini_object_ref (object);
}

gpointer
gst_mfx_object_pool_get_object (GstMfxObjectPool * pool)
{
    gpointer object;

    g_return_val_if_fail (pool != NULL, NULL);

    g_mutex_lock (&pool->mutex);
    object = gst_mfx_object_pool_get_object_unlocked (pool);
    g_mutex_unlock (&pool->mutex);
    return object;
}

/**
 * gst_mfx_object_pool_put_object:
 * @pool: a #GstMfxObjectPool
 * @object: the object to add back to the pool
 *
 * Pushes the @object back into the pool. The @object shall be
 * obtained from the @pool through gst_mfx_object_pool_get_object().
 * Calling this function with an arbitrary object yields undefined
 * behaviour.
 */
static void
gst_mfx_object_pool_put_object_unlocked (GstMfxObjectPool * pool,
    gpointer object)
{
    GList *elem;

    elem = g_list_find (pool->used_objects, object);
    if (!elem)
        return;

    gst_mfx_mini_object_unref (object);
    --pool->used_count;
    pool->used_objects = g_list_delete_link (pool->used_objects, elem);
    g_queue_push_tail (&pool->free_objects, object);
}

void
gst_mfx_object_pool_put_object (GstMfxObjectPool * pool, gpointer object)
{
    g_return_if_fail (pool != NULL);
    g_return_if_fail (object != NULL);

    g_mutex_lock (&pool->mutex);
    gst_mfx_object_pool_put_object_unlocked (pool, object);
    g_mutex_unlock (&pool->mutex);
}

/**
 * gst_mfx_object_pool_get_size:
 * @pool: a #GstMfxObjectPool
 *
 * Returns the number of free objects available in the pool.
 *
 * Return value: number of free objects in the pool
 */
guint
gst_mfx_object_pool_get_size (GstMfxObjectPool * pool)
{
    guint size;

    g_return_val_if_fail (pool != NULL, 0);

    g_mutex_lock (&pool->mutex);
    size = g_queue_get_length (&pool->free_objects);
    g_mutex_unlock (&pool->mutex);
    return size;
}

/**
 * gst_mfx_object_pool_reserve:
 * @pool: a #GstMfxObjectPool
 * @n: the number of objects to pre-allocate
 *
 * Pre-allocates up to @n objects in the pool. If @n is less than or
 * equal to the number of free and used objects in the pool, this call
 * has no effect. Otherwise, it is a request for allocation of
 * additional objects.
 *
 * Return value: %TRUE on success
 */
static gboolean
gst_mfx_object_pool_reserve_unlocked (GstMfxObjectPool * pool, guint n)
{
    guint i, num_allocated;

    num_allocated = gst_mfx_object_pool_get_size (pool) + pool->used_count;
    if (n < num_allocated)
        return TRUE;

    if ((n -= num_allocated) > pool->capacity)
        n = pool->capacity;

    for (i = num_allocated; i < n; i++) {
        gpointer object;

        g_mutex_unlock (&pool->mutex);
        object = gst_mfx_object_pool_alloc_object (pool);
        g_mutex_lock (&pool->mutex);
        if (!object)
            return FALSE;
        g_queue_push_tail (&pool->free_objects, object);
    }
    return TRUE;
}

gboolean
gst_mfx_object_pool_reserve (GstMfxObjectPool * pool, guint n)
{
    gboolean success;

    g_return_val_if_fail (pool != NULL, 0);

    g_mutex_lock (&pool->mutex);
    success = gst_mfx_object_pool_reserve_unlocked (pool, n);
    g_mutex_unlock (&pool->mutex);
    return success;
}

/**
 * gst_mfx_object_pool_get_capacity:
 * @pool: a #GstMfxObjectPool
 *
 * Returns the maximum number of objects in the pool. i.e. the maximum
 * number of objects that can be returned by gst_mfx_object_pool_get_object().
 *
 * Return value: the capacity of the pool
 */
guint
gst_mfx_object_pool_get_capacity (GstMfxObjectPool * pool)
{
    guint capacity;

    g_return_val_if_fail (pool != NULL, 0);

    g_mutex_lock (&pool->mutex);
    capacity = pool->capacity;
    g_mutex_unlock (&pool->mutex);

    return capacity;
}

/**
 * gst_mfx_object_pool_set_capacity:
 * @pool: a #GstMfxObjectPool
 * @capacity: the maximal capacity of the pool
 *
 * Sets the maximum number of objects that can be allocated in the pool.
 */
void
gst_mfx_object_pool_set_capacity (GstMfxObjectPool * pool, guint capacity)
{
    g_return_if_fail (pool != NULL);

    g_mutex_lock (&pool->mutex);
    pool->capacity = capacity;
    g_mutex_unlock (&pool->mutex);
}
