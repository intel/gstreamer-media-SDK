#ifndef GSTMFXDISPLAYCACHE_H
#define GSTMFXDISPLAYCACHE_H

#include "gstmfxdisplay.h"
#include "gstmfxminiobject.h"

typedef struct _GstMfxDisplayCache            GstMfxDisplayCache;

GstMfxDisplayCache *
gst_mfx_display_cache_new(void);

#define gst_mfx_display_cache_ref(cache) \
	((GstMfxDisplayCache *) gst_mfx_mini_object_ref ( \
	GST_MFX_MINI_OBJECT (cache)))
#define gst_mfx_display_cache_unref(cache) \
	gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (cache))
#define gst_mfx_display_cache_replace(old_cache_ptr, new_cache) \
	gst_mfx_mini_object_replace ((GstMfxMiniObject **) (old_cache_ptr), \
	GST_MFX_MINI_OBJECT (new_cache))

void
gst_mfx_display_cache_lock(GstMfxDisplayCache * cache);

void
gst_mfx_display_cache_unlock(GstMfxDisplayCache * cache);

gboolean
gst_mfx_display_cache_is_empty(GstMfxDisplayCache * cache);

gboolean
gst_mfx_display_cache_add(GstMfxDisplayCache * cache,
	GstMfxDisplayInfo * info);

void
gst_mfx_display_cache_remove(GstMfxDisplayCache * cache,
	GstMfxDisplay * display);

const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup(GstMfxDisplayCache
	* cache, GstMfxDisplay * display);

const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_custom(GstMfxDisplayCache * cache,
	GCompareFunc func, gconstpointer data, guint display_types);

const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_by_va_display(GstMfxDisplayCache * cache,
	VADisplay va_display);

const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_by_native_display(GstMfxDisplayCache *
	cache, gpointer native_display, guint display_types);

const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_by_name(GstMfxDisplayCache * cache,
	const gchar * display_name, guint display_types);

#endif /* GSTMFXDISPLAYCACHE_H */