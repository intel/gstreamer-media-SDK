#ifndef GST_MFX_OBJECT_POOL_H
#define GST_MFX_OBJECT_POOL_H

#include <glib.h>

G_BEGIN_DECLS

#define GST_MFX_OBJECT_POOL(obj) \
  ((GstMfxObjectPool *)(obj))

typedef struct _GstMfxObjectPool GstMfxObjectPool;

GstMfxObjectPool *
gst_mfx_object_pool_ref (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_unref (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_replace (GstMfxObjectPool ** old_pool_ptr,
    GstMfxObjectPool * new_pool);

gpointer
gst_mfx_object_pool_get_object (GstMfxObjectPool * pool);

void
gst_mfx_object_pool_put_object (GstMfxObjectPool * pool, gpointer object);

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
