#ifndef GST_MFX_OBJECT_POOL_PRIV_H
#define GST_MFX_OBJECT_POOL_PRIV_H

#include "gstmfxminiobject.h"

G_BEGIN_DECLS

#define GST_MFX_OBJECT_POOL_CLASS(klass) \
  ((GstMfxObjectPoolClass *)(klass))
#define GST_MFX_IS_OBJECT_POOL_CLASS(klass)    \
  ((klass) != NULL)

typedef struct _GstMfxObjectPoolClass GstMfxObjectPoolClass;

/**
 * GstMfxObjectPool:
 *
 * A pool of lazily allocated mfx objects. e.g. surfaces, images.
 */
struct _GstMfxObjectPool
{
  /*< private >*/
    GstMfxMiniObject parent_instance;

    guint object_type;
    //GstMfxDisplay *display;
    GQueue free_objects;
    GList *used_objects;
    guint used_count;
    guint capacity;
    GMutex mutex;
};

/**
 * GstMfxObjectPoolClass:
 * @alloc_object: virtual function for allocating a object pool object
 *
 * A pool base class used to hold object objects. e.g. surfaces, images.
 */
struct _GstMfxObjectPoolClass
{
    /*< private >*/
    GstMfxMiniObjectClass parent_class;

    /*< public >*/
    gpointer (*alloc_object) (GstMfxObjectPool * pool);
};

void
gst_mfx_object_pool_init (GstMfxObjectPool * pool, //GstMfxDisplay * display,
    GstMfxPoolObjectType object_type);

void
gst_mfx_object_pool_finalize (GstMfxObjectPool * pool);

/* Internal aliases */

#define gst_mfx_object_pool_ref_internal(pool) \
  ((gpointer)gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (pool)))

#define gst_mfx_object_pool_unref_internal(pool) \
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (pool))

#define gst_mfx_object_pool_replace_internal(old_pool_ptr, new_pool) \
  gst_mfx_mini_object_replace ((GstMfxMiniObject **)(old_pool_ptr),  \
      GST_MFX_MINI_OBJECT (new_pool))

#undef  gst_mfx_object_pool_ref
#define gst_mfx_object_pool_ref(pool) \
  gst_mfx_object_pool_ref_internal ((pool))

#undef  gst_mfx_object_pool_unref
#define gst_mfx_object_pool_unref(pool) \
   gst_mfx_object_pool_unref_internal ((pool))

#undef  gst_mfx_object_pool_replace
#define gst_mfx_object_pool_replace(old_pool_ptr, new_pool) \
    gst_mfx_object_pool_replace_internal ((old_pool_ptr), (new_pool))

G_END_DECLS

#endif /* GST_MFX_OBJECT_POOL_PRIV_H */

