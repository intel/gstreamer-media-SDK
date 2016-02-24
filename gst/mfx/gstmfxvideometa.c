#include "gstmfxvideometa.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxvideomemory.h"

#define GST_MFX_VIDEO_META(obj) \
	((GstMfxVideoMeta *) (obj))
#define GST_MFX_IS_VIDEO_META(obj) \
	(GST_MFX_VIDEO_META (obj) != NULL)

struct _GstMfxVideoMeta
{
	GstBuffer *buffer;
	gint ref_count;
	GstMfxSurfaceProxy *proxy;
};

static inline void
set_surface_proxy(GstMfxVideoMeta * meta, GstMfxSurfaceProxy * proxy)
{
	meta->proxy = gst_mfx_surface_proxy_ref(proxy);
}

static gboolean
set_surface_proxy_from_pool(GstMfxVideoMeta * meta, GstMfxSurfacePool * pool)
{
	GstMfxSurfaceProxy *proxy;

	proxy = gst_mfx_surface_proxy_new_from_pool(GST_MFX_SURFACE_POOL(pool));
	if (!proxy)
		return FALSE;

	set_surface_proxy(meta, proxy);
	gst_mfx_surface_proxy_unref(proxy);
	return TRUE;
}

static inline void
gst_mfx_video_meta_destroy_proxy(GstMfxVideoMeta * meta)
{
	gst_mfx_surface_proxy_replace(&meta->proxy, NULL);
}

static void
gst_mfx_video_meta_finalize(GstMfxVideoMeta * meta)
{
	gst_mfx_video_meta_destroy_proxy(meta);
}

static void
gst_mfx_video_meta_init(GstMfxVideoMeta * meta)
{
    meta->buffer = NULL;
    meta->ref_count = 1;
    meta->proxy = NULL;
}

static inline GstMfxVideoMeta *
_gst_mfx_video_meta_create(void)
{
	return g_slice_new(GstMfxVideoMeta);
}

static inline void
_gst_mfx_video_meta_destroy(GstMfxVideoMeta * meta)
{
	g_slice_free1(sizeof (*meta), meta);
}

static inline GstMfxVideoMeta *
_gst_mfx_video_meta_new(void)
{
	GstMfxVideoMeta *meta;

	meta = _gst_mfx_video_meta_create();
	if (!meta)
		return NULL;
	gst_mfx_video_meta_init(meta);
	return meta;
}

static inline void
_gst_mfx_video_meta_free(GstMfxVideoMeta * meta)
{
	g_atomic_int_inc(&meta->ref_count);

	gst_mfx_video_meta_finalize(meta);

	if (G_LIKELY(g_atomic_int_dec_and_test(&meta->ref_count)))
		_gst_mfx_video_meta_destroy(meta);
}

GstMfxVideoMeta *
gst_mfx_video_meta_copy(GstMfxVideoMeta * meta)
{
	GstMfxVideoMeta *copy;

	g_return_val_if_fail(GST_MFX_IS_VIDEO_META(meta), NULL);

	copy = _gst_mfx_video_meta_create();
	if (!copy)
		return NULL;

    copy->buffer = NULL;
    copy->ref_count = 1;
    copy->proxy = meta->proxy ? gst_mfx_surface_proxy_copy (meta->proxy) : NULL;

	return copy;
}

GstMfxVideoMeta *
gst_mfx_video_meta_new()
{
	GstMfxVideoMeta *meta;

	meta = _gst_mfx_video_meta_new();
	if (G_UNLIKELY(!meta))
		return NULL;

	return meta;
}

GstMfxVideoMeta *
gst_mfx_video_meta_new_from_pool(GstMfxSurfacePool * pool)
{
	GstMfxVideoMeta *meta;

	g_return_val_if_fail(pool != NULL, NULL);

	meta = _gst_mfx_video_meta_new();
	if (G_UNLIKELY(!meta))
		return NULL;

	if (!set_surface_proxy_from_pool(meta, pool))
		goto error;

	return meta;

error:
	gst_mfx_video_meta_unref(meta);
	return NULL;
}

GstMfxVideoMeta *
gst_mfx_video_meta_new_with_surface_proxy(GstMfxSurfaceProxy * proxy)
{
	GstMfxVideoMeta *meta;

	g_return_val_if_fail(proxy != NULL, NULL);

	meta = _gst_mfx_video_meta_new();
	if (G_UNLIKELY(!meta))
		return NULL;

	gst_mfx_video_meta_set_surface_proxy(meta, proxy);
	return meta;
}

GstMfxVideoMeta *
gst_mfx_video_meta_ref(GstMfxVideoMeta * meta)
{
	g_return_val_if_fail(meta != NULL, NULL);

	g_atomic_int_inc(&meta->ref_count);
	return meta;
}

void
gst_mfx_video_meta_unref(GstMfxVideoMeta * meta)
{
	g_return_if_fail(meta != NULL);
	g_return_if_fail(meta->ref_count > 0);

	if (g_atomic_int_dec_and_test(&meta->ref_count))
		_gst_mfx_video_meta_free(meta);
}

void
gst_mfx_video_meta_replace (GstMfxVideoMeta ** old_meta_ptr,
    GstMfxVideoMeta * new_meta)
{
    GstMfxVideoMeta *old_meta;

    g_return_if_fail (old_meta_ptr != NULL);

    old_meta = g_atomic_pointer_get ((gpointer *) old_meta_ptr);

    if (old_meta == new_meta)
        return;

    if (new_meta)
        gst_mfx_video_meta_ref (new_meta);

    while (!g_atomic_pointer_compare_and_exchange ((gpointer *) old_meta_ptr,
        old_meta, new_meta))
    old_meta = g_atomic_pointer_get ((gpointer *) old_meta_ptr);

    if (old_meta)
        gst_mfx_video_meta_unref (old_meta);
}

GstMfxSurfaceProxy *
gst_mfx_video_meta_get_surface_proxy(GstMfxVideoMeta * meta)
{
	g_return_val_if_fail(GST_MFX_IS_VIDEO_META(meta), NULL);

	return meta->proxy;
}

void
gst_mfx_video_meta_set_surface_proxy(GstMfxVideoMeta * meta,
	GstMfxSurfaceProxy * proxy)
{
	g_return_if_fail(GST_MFX_IS_VIDEO_META(meta));

	gst_mfx_video_meta_destroy_proxy(meta);

	if (proxy)
		set_surface_proxy(meta, proxy);
}

#define GST_MFX_VIDEO_META_HOLDER(meta) \
	((GstMfxVideoMetaHolder *) (meta))

typedef struct _GstMfxVideoMetaHolder GstMfxVideoMetaHolder;
struct _GstMfxVideoMetaHolder
{
	GstMeta base;
	GstMfxVideoMeta *meta;
};

static gboolean
gst_mfx_video_meta_holder_init(GstMfxVideoMetaHolder * meta,
	gpointer params, GstBuffer * buffer)
{
	meta->meta = NULL;
	return TRUE;
}

static void
gst_mfx_video_meta_holder_free(GstMfxVideoMetaHolder * meta,
	GstBuffer * buffer)
{
	if (meta->meta)
		gst_mfx_video_meta_unref(meta->meta);
}

static gboolean
gst_mfx_video_meta_holder_transform(GstBuffer * dst_buffer, GstMeta * meta,
	GstBuffer * src_buffer, GQuark type, gpointer data)
{
	GstMfxVideoMetaHolder *const src_meta = GST_MFX_VIDEO_META_HOLDER(meta);

	if (GST_META_TRANSFORM_IS_COPY(type)) {
		GstMfxVideoMeta *const dst_meta =
			gst_mfx_video_meta_copy(src_meta->meta);
		gst_buffer_set_mfx_video_meta(dst_buffer, dst_meta);
		gst_mfx_video_meta_unref(dst_meta);
		return TRUE;
	}
	return FALSE;
}

GType
gst_mfx_video_meta_api_get_type(void)
{
	static gsize g_type;
	static const gchar *tags[] = { "memory", NULL };

	if (g_once_init_enter(&g_type)) {
		GType type = gst_meta_api_type_register("GstMfxVideoMetaAPI", tags);
		g_once_init_leave(&g_type, type);
	}
	return g_type;
}

#define GST_MFX_VIDEO_META_INFO gst_mfx_video_meta_info_get ()
static const GstMetaInfo *
gst_mfx_video_meta_info_get(void)
{
	static gsize g_meta_info;

	if (g_once_init_enter(&g_meta_info)) {
		gsize meta_info =
			GPOINTER_TO_SIZE(gst_meta_register(GST_MFX_VIDEO_META_API_TYPE,
			"GstMfxVideoMeta", sizeof (GstMfxVideoMetaHolder),
			(GstMetaInitFunction)gst_mfx_video_meta_holder_init,
			(GstMetaFreeFunction)gst_mfx_video_meta_holder_free,
			(GstMetaTransformFunction)gst_mfx_video_meta_holder_transform));
		g_once_init_leave(&g_meta_info, meta_info);
	}
	return GSIZE_TO_POINTER(g_meta_info);
}

GstMfxVideoMeta *
gst_buffer_get_mfx_video_meta(GstBuffer * buffer)
{
	GstMfxVideoMeta *meta;
	GstMeta *m;

	g_return_val_if_fail(GST_IS_BUFFER(buffer), NULL);

	m = gst_buffer_get_meta(buffer, GST_MFX_VIDEO_META_API_TYPE);
	if (!m)
		return NULL;

	meta = GST_MFX_VIDEO_META_HOLDER(m)->meta;
	if (meta)
		meta->buffer = buffer;
	return meta;
}

void
gst_buffer_set_mfx_video_meta(GstBuffer * buffer, GstMfxVideoMeta * meta)
{
	GstMeta *m;

	g_return_if_fail(GST_IS_BUFFER(buffer));
	g_return_if_fail(GST_MFX_IS_VIDEO_META(meta));

	m = gst_buffer_add_meta(buffer, GST_MFX_VIDEO_META_INFO, NULL);
	if (m)
		GST_MFX_VIDEO_META_HOLDER(m)->meta = gst_mfx_video_meta_ref(meta);
}
