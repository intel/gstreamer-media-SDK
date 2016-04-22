#include "gstmfxvideomemory.h"

GST_DEBUG_CATEGORY_STATIC(gst_debug_mfxvideomemory);
#define GST_CAT_DEFAULT gst_debug_mfxvideomemory

static void gst_mfx_video_memory_reset_image (GstMfxVideoMemory * mem);

static guchar *
get_image_data (VaapiImage * image)
{
    guchar *data;

    data = vaapi_image_get_plane (image, 0);
    if (!data)
        return NULL;

    return data;
}

static VaapiImage *
new_image(GstMfxDisplay * display, const GstVideoInfo * vip)
{
    if (!GST_VIDEO_INFO_WIDTH (vip) || !GST_VIDEO_INFO_HEIGHT (vip))
        return NULL;
    return vaapi_image_new (display,
        GST_VIDEO_INFO_WIDTH (vip),
        GST_VIDEO_INFO_HEIGHT (vip),
        GST_VIDEO_INFO_FORMAT(vip));
}

static gboolean
ensure_image (GstMfxVideoMemory * mem)
{
    if (!mem->image) {
        mem->image = gst_mfx_surface_proxy_derive_image (mem->proxy);

        if (!mem->image) {
            GST_WARNING ("failed to derive image");
            return FALSE;
        }
    }

    return TRUE;
}


static GstMfxSurfaceProxy *
new_surface_proxy (GstMfxVideoMemory * mem)
{
    GstMfxVideoAllocator *const allocator =
        GST_MFX_VIDEO_ALLOCATOR_CAST (GST_MEMORY_CAST (mem)->allocator);

    return gst_mfx_surface_proxy_new_from_pool (
        GST_MFX_SURFACE_POOL (allocator->surface_pool));
}

static gboolean
ensure_surface (GstMfxVideoMemory * mem)
{
    if (!mem->proxy) {
        gst_mfx_surface_proxy_replace (&mem->proxy,
            gst_mfx_video_meta_get_surface_proxy (mem->meta));
    }

    if (!mem->proxy) {
        mem->proxy = new_surface_proxy (mem);
        if (!mem->proxy)
            return FALSE;
        gst_mfx_video_meta_set_surface_proxy (mem->meta, mem->proxy);
    }

    return GST_MFX_SURFACE_PROXY_SURFACE (mem->proxy) != NULL;
}

gboolean
gst_video_meta_map_mfx_surface(GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags)
{
	GstMfxVideoMemory *const mem =
		GST_MFX_VIDEO_MEMORY_CAST(gst_buffer_peek_memory(meta->buffer, 0));

	g_return_val_if_fail(mem, FALSE);
	g_return_val_if_fail(GST_MFX_IS_VIDEO_ALLOCATOR(mem->parent_instance.
		allocator), FALSE);
	g_return_val_if_fail(mem->meta, FALSE);

    /* Map for writing */
	if (++mem->map_count == 1) {
		if (!ensure_surface(mem))
			goto error_ensure_surface;

		if (GST_MFX_SURFACE_PROXY_MEMID(mem->proxy) != GST_MFX_ID_INVALID) {
			if (!ensure_image(mem))
				goto error_ensure_image;

			// Load VA image from surface
			if (!vaapi_image_map(mem->image))
				goto error_map_image;
		}
	}

	if (!mem->image) {
		*data = gst_mfx_surface_proxy_get_plane(mem->proxy, plane);
		*stride = gst_mfx_surface_proxy_get_pitch(mem->proxy, plane);
	}
	else {
		*data = vaapi_image_get_plane(mem->image, plane);
		*stride = vaapi_image_get_pitch(mem->image, plane);
	}


	info->flags = flags;
	return TRUE;

	/* ERRORS */
error_ensure_surface:
	{
		const GstVideoInfo *const vip = mem->image_info;
		GST_ERROR("failed to create surface of size %ux%u",
			GST_VIDEO_INFO_WIDTH(vip), GST_VIDEO_INFO_HEIGHT(vip));
		return FALSE;
	}
error_ensure_image:
	{
		const GstVideoInfo *const vip = mem->image_info;
		GST_ERROR("failed to create image of size %ux%u",
			GST_VIDEO_INFO_WIDTH(vip), GST_VIDEO_INFO_HEIGHT(vip));
		return FALSE;
	}
error_map_image:
	{
		GST_ERROR("failed to map image");
		return FALSE;
	}

	return TRUE;
}

gboolean
gst_video_meta_unmap_mfx_surface(GstVideoMeta * meta, guint plane,
    GstMapInfo * info)
{
	GstMfxVideoMemory *const mem =
		GST_MFX_VIDEO_MEMORY_CAST(gst_buffer_peek_memory(meta->buffer, 0));

	g_return_val_if_fail(mem, FALSE);
	g_return_val_if_fail(GST_MFX_IS_VIDEO_ALLOCATOR(mem->parent_instance.
		allocator), FALSE);
	g_return_val_if_fail(mem->meta, FALSE);
	g_return_val_if_fail(mem->proxy, FALSE);

	if (--mem->map_count == 0) {
		/* Unmap VA image used for read/writes */
		if (mem->image && info->flags & GST_MAP_READWRITE) {
			vaapi_image_unmap(mem->image);
		}
	}

	return TRUE;
}

GstMemory *
gst_mfx_video_memory_new(GstAllocator * base_allocator,
	GstMfxVideoMeta * meta)
{
	GstMfxVideoAllocator *const allocator =
		GST_MFX_VIDEO_ALLOCATOR_CAST(base_allocator);

	const GstVideoInfo *vip;
	GstMfxVideoMemory *mem;

	g_return_val_if_fail(GST_MFX_IS_VIDEO_ALLOCATOR(allocator), NULL);

	mem = g_slice_new(GstMfxVideoMemory);
	if (!mem)
		return NULL;

	vip = &allocator->image_info;
	gst_memory_init(&mem->parent_instance, GST_MEMORY_FLAG_NO_SHARE,
		gst_object_ref(allocator), NULL, GST_VIDEO_INFO_SIZE(vip), 0,
		0, GST_VIDEO_INFO_SIZE(vip));

	mem->proxy = NULL;
    mem->image_info = &allocator->image_info;
	mem->image = NULL;
	mem->meta = meta ? gst_mfx_video_meta_ref(meta) : NULL;
	mem->map_type = 0;
	mem->map_count = 0;

	return GST_MEMORY_CAST(mem);
}

static void
gst_mfx_video_memory_free (GstMfxVideoMemory * mem)
{
    gst_mfx_video_memory_reset_image (mem);
    gst_mfx_surface_proxy_replace (&mem->proxy, NULL);
    gst_mfx_video_meta_replace (&mem->meta, NULL);
    gst_object_unref (GST_MEMORY_CAST (mem)->allocator);
    g_slice_free (GstMfxVideoMemory, mem);
}

void
gst_mfx_video_memory_reset_image (GstMfxVideoMemory * mem)
{
    if (mem->image)
        gst_mfx_mini_object_replace(&mem->image, NULL);
}

void
gst_mfx_video_memory_reset_surface (GstMfxVideoMemory * mem)
{
    gst_mfx_video_memory_reset_image (mem);
    gst_mfx_surface_proxy_replace (&mem->proxy, NULL);
    if (mem->meta)
        gst_mfx_video_meta_set_surface_proxy (mem->meta, NULL);
}

static gpointer
gst_mfx_video_memory_map (GstMfxVideoMemory * mem, gsize maxsize,
    guint flags)
{
    gpointer data;

    g_return_val_if_fail (mem, NULL);
    g_return_val_if_fail (mem->meta, NULL);

    if (mem->map_count == 0) {
        switch (flags & GST_MAP_READWRITE) {
        case 0:
            // No flags set: return a GstMfxSurfaceProxy
            gst_mfx_surface_proxy_replace (&mem->proxy,
                gst_mfx_video_meta_get_surface_proxy (mem->meta));
            if (!mem->proxy)
                goto error_no_surface_proxy;
            mem->map_type = GST_MFX_VIDEO_MEMORY_MAP_TYPE_SURFACE;
            break;
        case GST_MAP_READ:
            // Only read flag set: return raw pixels
            if (!ensure_surface (mem))
                goto error_no_surface;
            if ((GST_MFX_SURFACE_PROXY_MEMID(mem->proxy) != GST_MFX_ID_INVALID)) {
                if (!ensure_image (mem))
                    goto error_no_image;
                if (!vaapi_image_map (mem->image))
                    goto error_map_image;
                mem->map_type = GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR;
            }
            else
                mem->map_type = GST_MFX_SYSTEM_MEMORY_MAP_TYPE_LINEAR;
            break;
        default:
            goto error_unsupported_map;
        }
    }

    switch (mem->map_type) {
        case GST_MFX_VIDEO_MEMORY_MAP_TYPE_SURFACE:
            if (!mem->proxy)
                goto error_no_surface_proxy;
            data = mem->proxy;
            break;
        case GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR:
            if (!mem->image)
                goto error_no_image;
            data = get_image_data (mem->image);
            break;
        case GST_MFX_SYSTEM_MEMORY_MAP_TYPE_LINEAR:
            data = gst_mfx_surface_proxy_get_data(mem->proxy);
            break;
        default:
            goto error_unsupported_map_type;
    }
    mem->map_count++;
    return data;

  /* ERRORS */
error_unsupported_map:
    GST_ERROR ("unsupported map flags (0x%x)", flags);
    return NULL;
error_unsupported_map_type:
    GST_ERROR ("unsupported map type (%d)", mem->map_type);
    return NULL;
error_no_surface_proxy:
    GST_ERROR ("failed to extract GstMfxSurfaceProxy from video meta");
    return NULL;
error_no_surface:
    GST_ERROR ("failed to extract VA surface from video buffer");
    return NULL;
error_no_image:
    GST_ERROR ("failed to extract VA image from video buffer");
    return NULL;
error_map_image:
    GST_ERROR ("failed to map VA image");
    return NULL;
}

static void
gst_mfx_video_memory_unmap (GstMfxVideoMemory * mem)
{
    if (mem->map_count == 1) {
        switch (mem->map_type) {
            case GST_MFX_VIDEO_MEMORY_MAP_TYPE_SURFACE:
                gst_mfx_surface_proxy_replace (&mem->proxy, NULL);
                break;
            case GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR:
                vaapi_image_unmap (mem->image);
                break;
            case GST_MFX_SYSTEM_MEMORY_MAP_TYPE_LINEAR:
                break;
            default:
                goto error_incompatible_map;
        }
        mem->map_type = 0;
    }
    mem->map_count--;
    return;

  /* ERRORS */
error_incompatible_map:
    GST_ERROR ("incompatible map type (%d)", mem->map_type);
    return;
}

static GstMfxVideoMemory *
gst_mfx_video_memory_copy (GstMfxVideoMemory * mem,
    gssize offset, gssize size)
{
    GstMfxVideoMeta *meta;
    GstMemory *out_mem;
    gsize maxsize;

    g_return_val_if_fail (mem, NULL);
    g_return_val_if_fail (mem->meta, NULL);

    /* XXX: this implements a soft-copy, i.e. underlying VA surfaces
        are not copied */
    (void) gst_memory_get_sizes (GST_MEMORY_CAST (mem), NULL, &maxsize);
    if (offset != 0 || (size != -1 && (gsize) size != maxsize))
        goto error_unsupported;

    meta = gst_mfx_video_meta_copy (mem->meta);
    if (!meta)
        goto error_allocate_memory;

    out_mem = gst_mfx_video_memory_new (GST_MEMORY_CAST (mem)->allocator, meta);
    gst_mfx_video_meta_unref (meta);
    if (!out_mem)
        goto error_allocate_memory;
    return GST_MFX_VIDEO_MEMORY_CAST (out_mem);

  /* ERRORS */
error_unsupported:
    GST_ERROR ("failed to copy partial memory (unsupported operation)");
    return NULL;
error_allocate_memory:
    GST_ERROR ("failed to allocate GstMfxVideoMemory copy");
    return NULL;
}

static GstMfxVideoMemory *
gst_mfx_video_memory_share (GstMfxVideoMemory * mem,
    gssize offset, gssize size)
{
    GST_FIXME ("unimplemented GstMfxVideoAllocator::mem_share() hook");
    return NULL;
}

static gboolean
gst_mfx_video_memory_is_span (GstMfxVideoMemory * mem1,
    GstMfxVideoMemory * mem2, gsize * offset_ptr)
{
    GST_FIXME ("unimplemented GstMfxVideoAllocator::mem_is_span() hook");
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/* --- GstMfxVideoAllocator                                           --- */
/* ------------------------------------------------------------------------ */

#define GST_MFX_VIDEO_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_MFX_TYPE_VIDEO_ALLOCATOR, \
      GstMfxVideoAllocatorClass))

#define GST_MFX_IS_VIDEO_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_MFX_TYPE_VIDEO_ALLOCATOR))

G_DEFINE_TYPE (GstMfxVideoAllocator,
    gst_mfx_video_allocator, GST_TYPE_ALLOCATOR);

static GstMemory *
gst_mfx_video_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
    g_warning ("use gst_mfx_video_memory_new() to allocate from "
      "GstMfxVideoMemory allocator");

    return NULL;
}

static void
gst_mfx_video_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
    gst_mfx_video_memory_free (GST_MFX_VIDEO_MEMORY_CAST (mem));
}

static void
gst_mfx_video_allocator_finalize (GObject * object)
{
    GstMfxVideoAllocator *const allocator =
        GST_MFX_VIDEO_ALLOCATOR_CAST (object);

    gst_mfx_surface_pool_replace (&allocator->surface_pool, NULL);

    G_OBJECT_CLASS (gst_mfx_video_allocator_parent_class)->finalize (object);
}

static void
gst_mfx_video_allocator_class_init (GstMfxVideoAllocatorClass * klass)
{
    GObjectClass *const object_class = G_OBJECT_CLASS (klass);
    GstAllocatorClass *const allocator_class = GST_ALLOCATOR_CLASS (klass);

    GST_DEBUG_CATEGORY_INIT (gst_debug_mfxvideomemory,
        "mfxvideomemory", 0, "MFX video memory allocator");

    object_class->finalize = gst_mfx_video_allocator_finalize;
    allocator_class->alloc = gst_mfx_video_allocator_alloc;
    allocator_class->free = gst_mfx_video_allocator_free;
}

static void
gst_mfx_video_allocator_init (GstMfxVideoAllocator * allocator)
{
    GstAllocator *const base_allocator = GST_ALLOCATOR_CAST (allocator);

    base_allocator->mem_type = GST_MFX_VIDEO_MEMORY_NAME;
    base_allocator->mem_map = (GstMemoryMapFunction)
      gst_mfx_video_memory_map;
    base_allocator->mem_unmap = (GstMemoryUnmapFunction)
      gst_mfx_video_memory_unmap;
    base_allocator->mem_copy = (GstMemoryCopyFunction)
      gst_mfx_video_memory_copy;
    base_allocator->mem_share = (GstMemoryShareFunction)
      gst_mfx_video_memory_share;
    base_allocator->mem_is_span = (GstMemoryIsSpanFunction)
      gst_mfx_video_memory_is_span;
}

static gboolean
gst_video_info_update_from_image (GstVideoInfo * vip, VaapiImage * image)
{
    GstVideoFormat format;
    const guchar *data;
    guint i, num_planes, data_size, width, height;

    /* Reset format from image */
    format = vaapi_image_get_format (image);
    vaapi_image_get_size (image, &width, &height);
    gst_video_info_set_format (vip, format, width, height);

    num_planes = vaapi_image_get_plane_count (image);
    g_return_val_if_fail (num_planes == GST_VIDEO_INFO_N_PLANES (vip), FALSE);

    /* Determine the base data pointer */
    data = get_image_data (image);
    g_return_val_if_fail (data != NULL, FALSE);
    data_size = vaapi_image_get_data_size (image);

    /* Check that we don't have disjoint planes */
    for (i = 0; i < num_planes; i++) {
        const guchar *const plane = vaapi_image_get_plane (image, i);
        if (plane - data > data_size)
            return FALSE;
    }

    /* Update GstVideoInfo structure */
    for (i = 0; i < num_planes; i++) {
        const guchar *const plane = vaapi_image_get_plane (image, i);
        GST_VIDEO_INFO_PLANE_OFFSET (vip, i) = plane - data;
        GST_VIDEO_INFO_PLANE_STRIDE (vip, i) = vaapi_image_get_pitch (image, i);
    }
    GST_VIDEO_INFO_SIZE (vip) = data_size;
    return TRUE;
}

static inline void
allocator_configure_image_info(GstMfxDisplay * display,
    GstMfxVideoAllocator * allocator)
{
    VaapiImage *image = NULL;

    image = new_image (display, &allocator->video_info);
    if (!image)
        goto bail;
    if (!vaapi_image_map (image))
        goto bail;

    gst_video_info_update_from_image (&allocator->image_info, image);
    vaapi_image_unmap (image);

bail:
    if (image)
        gst_mfx_mini_object_unref (image);
}

GstAllocator *
gst_mfx_video_allocator_new(GstMfxDisplay * display,
	const GstVideoInfo * vip, gboolean mapped)
{
	GstMfxVideoAllocator *allocator;

    g_return_val_if_fail(display != NULL, NULL);
	g_return_val_if_fail(vip != NULL, NULL);

	allocator = g_object_new(GST_MFX_TYPE_VIDEO_ALLOCATOR, NULL);
	if (!allocator)
		return NULL;

	allocator->video_info = *vip;

    allocator_configure_image_info (display, allocator);
    allocator->surface_pool = gst_mfx_surface_pool_new (display,
        &allocator->image_info, mapped);
    if (!allocator->surface_pool)
        goto error_create_surface_pool;

	gst_allocator_set_mfx_video_info(GST_ALLOCATOR_CAST(allocator),
		&allocator->image_info, 0);

	return GST_ALLOCATOR_CAST(allocator);
  /* ERRORS */
error_create_surface_pool:
    {
        GST_ERROR ("failed to allocate MFX surface pool");
        gst_object_unref (allocator);
        return NULL;
    }
}


/* ------------------------------------------------------------------------ */
/* --- GstMfxVideoInfo = { GstVideoInfo, flags }                      --- */
/* ------------------------------------------------------------------------ */

static GstVideoInfo *
gst_mfx_video_info_copy (const GstVideoInfo * vip)
{
    GstVideoInfo *out_vip;

    out_vip = g_slice_new (GstVideoInfo);
    if (!out_vip)
        return NULL;

    gst_video_info_init (out_vip);
    *out_vip = *vip;
    return out_vip;
}

static void
gst_mfx_video_info_free (GstVideoInfo * vip)
{
    g_slice_free (GstVideoInfo, vip);
}

#define GST_MFX_TYPE_VIDEO_INFO gst_mfx_video_info_get_type ()
static GType
gst_mfx_video_info_get_type (void)
    {
    static gsize g_type;

    if (g_once_init_enter (&g_type)) {
        GType type;
        type = g_boxed_type_register_static ("GstMfxVideoInfo",
            (GBoxedCopyFunc) gst_mfx_video_info_copy,
            (GBoxedFreeFunc) gst_mfx_video_info_free);
        g_once_init_leave (&g_type, type);
    }
    return (GType) g_type;
}

#define GST_MFX_VIDEO_INFO_QUARK gst_mfx_video_info_quark_get ()
static GQuark
gst_mfx_video_info_quark_get (void)
{
    static gsize g_quark;

    if (g_once_init_enter (&g_quark)) {
        gsize quark = (gsize) g_quark_from_static_string ("GstMfxVideoInfo");
        g_once_init_leave (&g_quark, quark);
    }
    return g_quark;
}

#define INFO_QUARK info_quark_get ()
static GQuark
info_quark_get (void)
{
    static gsize g_quark;

    if (g_once_init_enter (&g_quark)) {
        gsize quark = (gsize) g_quark_from_static_string ("info");
        g_once_init_leave (&g_quark, quark);
    }
    return g_quark;
}

#define FLAGS_QUARK flags_quark_get ()
static GQuark
flags_quark_get (void)
{
    static gsize g_quark;

    if (g_once_init_enter (&g_quark)) {
        gsize quark = (gsize) g_quark_from_static_string ("flags");
        g_once_init_leave (&g_quark, quark);
    }
    return g_quark;
}

const GstVideoInfo *
gst_allocator_get_mfx_video_info (GstAllocator * allocator,
    guint * out_flags_ptr)
{
    const GstStructure *structure;
    const GValue *value;

    g_return_val_if_fail (GST_IS_ALLOCATOR (allocator), NULL);

    structure =
        g_object_get_qdata (G_OBJECT (allocator), GST_MFX_VIDEO_INFO_QUARK);
    if (!structure)
        return NULL;

    if (out_flags_ptr) {
    value = gst_structure_id_get_value (structure, FLAGS_QUARK);
    if (!value)
        return NULL;
        *out_flags_ptr = g_value_get_uint (value);
    }

    value = gst_structure_id_get_value (structure, INFO_QUARK);
    if (!value)
        return NULL;
    return g_value_get_boxed (value);
}

gboolean
gst_allocator_set_mfx_video_info (GstAllocator * allocator,
    const GstVideoInfo * vip, guint flags)
{
  g_return_val_if_fail (GST_IS_ALLOCATOR (allocator), FALSE);
  g_return_val_if_fail (vip != NULL, FALSE);

  g_object_set_qdata_full (G_OBJECT (allocator), GST_MFX_VIDEO_INFO_QUARK,
      gst_structure_new_id (GST_MFX_VIDEO_INFO_QUARK,
          INFO_QUARK, GST_MFX_TYPE_VIDEO_INFO, vip,
          FLAGS_QUARK, G_TYPE_UINT, flags, NULL),
      (GDestroyNotify) gst_structure_free);

  return TRUE;
}
