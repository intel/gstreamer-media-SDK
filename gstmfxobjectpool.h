
#ifndef GST_MFX_OBJECT_POOL_H
#define GST_MFX_OBJECT_POOL_H

#include <glib.h>

G_BEGIN_DECLS

#define GST_MFX_OBJECT_POOL(obj) \
  ((GstMfxObjectPool *)(obj))

typedef struct _GstMfxObjectPool GstMfxObjectPool;

/**
 * GstMfxPoolObjectType:
 * @GST_MFX_POOL_OBJECT_TYPE_IMAGE: #GstMfxImage objects.
 * @GST_MFX_POOL_OBJECT_TYPE_SURFACE: #GstMfxSurface objects.
 *
 * The set of all supported #GstMfxObjectPool object types.
 */
typedef enum
{
  GST_MFX_POOL_OBJECT_TYPE_IMAGE = 1,
  GST_MFX_POOL_OBJECT_TYPE_SURFACE,
} GstMfxPoolObjectType;

GstMfxObjectPool *
gst_mfx_object_pool_ref (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_unref (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_replace (GstMfxObjectPool ** old_pool_ptr,
    GstMfxObjectPool * new_pool);

//GstMfxDisplay *
//gst_mfx_object_pool_get_display (GstMfxObjectPool * pool);

GstMfxPoolObjectType
gst_mfx_object_pool_get_object_type (GstMfxObjectPool * pool);

gpointer
gst_mfx_object_pool_get_object (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_put_object (GstMfxObjectPool * pool, gpointer object);

gboolean
gst_mfx_object_pool_add_object (GstMfxObjectPool * pool, gpointer object);

gboolean
gst_mfx_object_pool_add_objects (GstMfxObjectPool * pool,
    GPtrArray * objects);

guint
gst_mfx_object_pool_get_size (GstMfxObjectPool * pool);

gboolean
gst_mfx_object_pool_reserve (GstMfxObjectPool * pool, guint n);

guint
gst_mfx_object_pool_get_capacity (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_set_capacity (GstMfxObjectPool * pool, guint capacity);

G_END_DECLS

#endif /* GST_MFX_OBJECT_POOL_H */
