#include <glib.h>
#include <string.h>
#include "gstmfxdisplaycache.h"

typedef struct _CacheEntry CacheEntry;
struct _CacheEntry
{
	GstMfxDisplayInfo info;
};

struct _GstMfxDisplayCache
{
	GstMfxMiniObject parent_instance;
	GRecMutex mutex;
	GList *list;
};

static void
cache_entry_free(CacheEntry * entry)
{
	GstMfxDisplayInfo *info;

	if (!entry)
		return;

	info = &entry->info;

	if (info->display_name) {
		g_free(info->display_name);
		info->display_name = NULL;
	}
	g_slice_free(CacheEntry, entry);
}

static CacheEntry *
cache_entry_new(const GstMfxDisplayInfo * di)
{
	GstMfxDisplayInfo *info;
	CacheEntry *entry;

	entry = g_slice_new(CacheEntry);
	if (!entry)
		return NULL;

	info = &entry->info;
	info->display = di->display;
	info->va_display = di->va_display;
	info->native_display = di->native_display;
	info->display_type = di->display_type;
	info->display_name = NULL;

	if (di->display_name) {
		info->display_name = g_strdup(di->display_name);
		if (!info->display_name)
			goto error;
	}
	return entry;

error:
	cache_entry_free(entry);
	return NULL;
}

static inline gboolean
is_compatible_display_type(const GstMfxDisplayType display_type,
guint display_types)
{
	if (display_type == GST_MFX_DISPLAY_TYPE_ANY)
		return TRUE;
	if (display_types == GST_MFX_DISPLAY_TYPE_ANY)
		return TRUE;
	return ((1U << display_type) & display_types) != 0;
}

static GList *
cache_lookup_1(GstMfxDisplayCache * cache, GCompareFunc func,
gconstpointer data, guint display_types)
{
	GList *l;

	for (l = cache->list; l != NULL; l = l->next) {
		GstMfxDisplayInfo *const info = &((CacheEntry *)l->data)->info;
		if (!is_compatible_display_type(info->display_type, display_types))
			continue;
		if (func(info, data))
			break;
	}
	return l;
}

static inline const GstMfxDisplayInfo *
cache_lookup(GstMfxDisplayCache * cache, GCompareFunc func,
gconstpointer data, guint display_types)
{
	GList *const m = cache_lookup_1(cache, func, data, display_types);

	return m ? &((CacheEntry *)m->data)->info : NULL;
}

static gint
compare_display(gconstpointer a, gconstpointer display)
{
	const GstMfxDisplayInfo *const info = a;

	return info->display == display;
}

static gint
compare_va_display(gconstpointer a, gconstpointer va_display)
{
	const GstMfxDisplayInfo *const info = a;

	return info->va_display == va_display;
}

static gint
compare_native_display(gconstpointer a, gconstpointer native_display)
{
	const GstMfxDisplayInfo *const info = a;

	return info->native_display == native_display;
}

static gint
compare_display_name(gconstpointer a, gconstpointer b)
{
	const GstMfxDisplayInfo *const info = a;
	const gchar *const display_name = b;

	if (info->display_name == NULL && display_name == NULL)
		return TRUE;
	if (!info->display_name || !display_name)
		return FALSE;
	return strcmp(info->display_name, display_name) == 0;
}

static void
gst_mfx_display_cache_finalize(GstMfxDisplayCache * cache)
{
	GList *l;

	if (cache->list) {
		for (l = cache->list; l != NULL; l = l->next)
			cache_entry_free(l->data);
		g_list_free(cache->list);
		cache->list = NULL;
	}
	g_rec_mutex_clear(&cache->mutex);
}

static const GstMfxMiniObjectClass *
gst_mfx_display_cache_class(void)
{
	static const GstMfxMiniObjectClass GstMfxDisplayCacheClass = {
		.size = sizeof (GstMfxDisplayCache),
		.finalize = (GDestroyNotify)gst_mfx_display_cache_finalize
	};
	return &GstMfxDisplayCacheClass;
}

/**
* gst_mfx_display_cache_new:
*
* Creates a new VA display cache.
*
* Return value: the newly created #GstMfxDisplayCache object
*/
GstMfxDisplayCache *
gst_mfx_display_cache_new(void)
{
	GstMfxDisplayCache *cache;

	cache = (GstMfxDisplayCache *)
		gst_mfx_mini_object_new(gst_mfx_display_cache_class());
	if (!cache)
		return NULL;

	g_rec_mutex_init(&cache->mutex);
	cache->list = NULL;
	return cache;
}

/**
* gst_mfx_display_cache_lock:
* @cache: the #GstMfxDisplayCache
*
* Locks the display cache @cache.
*/
void
gst_mfx_display_cache_lock(GstMfxDisplayCache * cache)
{
	g_return_if_fail(cache != NULL);

	g_rec_mutex_lock(&cache->mutex);
}

/**
* gst_mfx_display_cache_unlock:
* @cache: the #GstMfxDisplayCache
*
* Unlocks the display cache @cache.
*/
void
gst_mfx_display_cache_unlock(GstMfxDisplayCache * cache)
{
	g_return_if_fail(cache != NULL);

	g_rec_mutex_unlock(&cache->mutex);
}

/**
* gst_mfx_display_cache_is_empty:
* @cache: the #GstMfxDisplayCache
*
* Checks whether the display cache @cache is empty.
*
* Return value: %TRUE if the display @cache is empty, %FALSE otherwise.
*/
gboolean
gst_mfx_display_cache_is_empty(GstMfxDisplayCache * cache)
{
	g_return_val_if_fail(cache != NULL, 0);

	return cache->list == NULL;
}

/**
* gst_mfx_display_cache_add:
* @cache: the #GstMfxDisplayCache
* @info: the display cache info to add
*
* Adds a new entry with data from @info. The display @info data is
* copied into the newly created cache entry.
*
* Return value: %TRUE on success
*/
gboolean
gst_mfx_display_cache_add(GstMfxDisplayCache * cache,
	GstMfxDisplayInfo * info)
{
	CacheEntry *entry;

	g_return_val_if_fail(cache != NULL, FALSE);
	g_return_val_if_fail(info != NULL, FALSE);

	entry = cache_entry_new(info);
	if (!entry)
		return FALSE;

	cache->list = g_list_prepend(cache->list, entry);
	return TRUE;
}

/**
* gst_mfx_display_cache_remove:
* @cache: the #GstMfxDisplayCache
* @display: the display to remove from cache
*
* Removes any cache entry that matches the specified #GstMfxDisplay.
*/
void
gst_mfx_display_cache_remove(GstMfxDisplayCache * cache,
GstMfxDisplay * display)
{
	GList *m;

	m = cache_lookup_1(cache, compare_display, display,
		GST_MFX_DISPLAY_TYPE_ANY);
	if (!m)
		return;

	cache_entry_free(m->data);
	cache->list = g_list_delete_link(cache->list, m);
}

/**
* gst_mfx_display_cache_lookup:
* @cache: the #GstMfxDisplayCache
* @display: the display to find
*
* Looks up the display cache for the specified #GstMfxDisplay.
*
* Return value: a #GstMfxDisplayInfo matching @display, or %NULL if
*   none was found
*/
const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup(GstMfxDisplayCache * cache,
GstMfxDisplay * display)
{
	g_return_val_if_fail(cache != NULL, NULL);
	g_return_val_if_fail(display != NULL, NULL);

	return cache_lookup(cache, compare_display, display,
		GST_MFX_DISPLAY_TYPE_ANY);
}

/**
* gst_mfx_display_cache_lookup_custom:
* @cache: the #GstMfxDisplayCache
* @func: an comparison function
* @data: user data passed to the function
*
* Looks up an element in the display @cache using the supplied
* function @func to find the desired element. It iterates over all
* elements in cache, calling the given function which should return
* %TRUE when the desired element is found.
*
* The comparison function takes two gconstpointer arguments, a
* #GstMfxDisplayInfo as the first argument, and that is used to
* compare against the given user @data argument as the second
* argument.
*
* Return value: a #GstMfxDisplayInfo causing @func to succeed
*   (i.e. returning %TRUE), or %NULL if none was found
*/
const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_custom(GstMfxDisplayCache * cache,
GCompareFunc func, gconstpointer data, guint display_types)
{
	g_return_val_if_fail(cache != NULL, NULL);
	g_return_val_if_fail(func != NULL, NULL);

	return cache_lookup(cache, func, data, display_types);
}

/**
* gst_mfx_display_cache_lookup_by_va_display:
* @cache: the #GstMfxDisplayCache
* @va_display: the VA display to find
*
* Looks up the display cache for the specified VA display.
*
* Return value: a #GstMfxDisplayInfo matching @va_display, or %NULL
*   if none was found
*/
const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_by_va_display(GstMfxDisplayCache * cache,
	VADisplay va_display)
{
	g_return_val_if_fail(cache != NULL, NULL);
	g_return_val_if_fail(va_display != NULL, NULL);

	return cache_lookup(cache, compare_va_display, va_display,
		GST_MFX_DISPLAY_TYPE_ANY);
}

/**
* gst_mfx_display_cache_lookup_by_native_display:
* @cache: the #GstMfxDisplayCache
* @native_display: the native display to find
*
* Looks up the display cache for the specified native display.
*
* Return value: a #GstMfxDisplayInfo matching @native_display, or
*   %NULL if none was found
*/
const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_by_native_display(GstMfxDisplayCache * cache,
	gpointer native_display, guint display_types)
{
	g_return_val_if_fail(cache != NULL, NULL);
	g_return_val_if_fail(native_display != NULL, NULL);

	return cache_lookup(cache, compare_native_display, native_display,
		display_types);
}

/**
* gst_mfx_display_cache_lookup_by_name:
* @cache: the #GstMfxDisplayCache
* @display_name: the display name to match
*
* Looks up the display cache for the specified display name.
*
* Return value: a #GstMfxDisplayInfo matching @display_name, or
*   %NULL if none was found
*/
const GstMfxDisplayInfo *
gst_mfx_display_cache_lookup_by_name(GstMfxDisplayCache * cache,
	const gchar * display_name, guint display_types)
{
	g_return_val_if_fail(cache != NULL, NULL);

	return cache_lookup(cache, compare_display_name, display_name,
		display_types);
}