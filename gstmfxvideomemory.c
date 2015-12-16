#include "gstmfxvideomemory.h"

GST_DEBUG_CATEGORY_STATIC(gst_debug_mfxvideomemory);
#define GST_CAT_DEFAULT gst_debug_mfxvideomemory

#if 0

static guchar *
get_image_data (GstVaapiImage * image)
{
    guchar *data;
    VAImage va_image;

    data = gst_vaapi_image_get_plane (image, 0);
    if (!data || !gst_vaapi_image_get_image (image, &va_image))
        return NULL;

    data -= va_image.offsets[0];
    return data;
}

static GstVaapiImage *
new_image (VADisplay display, const GstVideoInfo * vip)
{
    if (!GST_VIDEO_INFO_WIDTH (vip) || !GST_VIDEO_INFO_HEIGHT (vip))
        return NULL;
    return gst_vaapi_image_new (display,
        GST_VIDEO_INFO_WIDTH (vip), GST_VIDEO_INFO_HEIGHT (vip));
}

static gboolean
ensure_image (GstMfxVideoMemory * mem)
{
    if (!mem->image) {
        mem->image = gst_mfx_surface_derive_image (mem->surface);

        if (!mem->image) {
            GST_WARNING ("failed to derive image");
            return FALSE;
        }
    }

    if (!mem->image) {
        GstMfxVideoAllocator *const allocator =
            GST_MFX_VIDEO_ALLOCATOR_CAST (GST_MEMORY_CAST (mem)->allocator);

        mem->image = gst_mfx_object_pool_get_object (allocator->image_pool);
        if (!mem->image)
            return FALSE;
    }

    gst_mfx_video_meta_set_image (mem->meta, mem->image);
    return TRUE;
}

static GstMfxSurfaceProxy *
new_surface_proxy (GstMfxVideoMemory * mem)
{
    GstMfxVideoAllocator *const allocator =
        GST_MFX_VIDEO_ALLOCATOR_CAST (GST_MEMORY_CAST (mem)->allocator);

    return
        gst_mfx_surface_proxy_new_from_pool (GST_MFX_SURFACE_POOL
            (allocator->surface_pool));
}

static gboolean
ensure_surface (GstMfxVideoMemory * mem)
{
    if (!mem->proxy) {
        gst_mfx_surface_proxy_replace (&mem->proxy,
            gst_mfx_video_meta_get_surface_proxy (mem->meta));

        if (!mem->proxy) {
            mem->proxy = new_surface_proxy (mem);
            if (!mem->proxy)
                return FALSE;
            gst_mfx_video_meta_set_surface_proxy (mem->meta, mem->proxy);
        }
    }
    mem->surface = GST_MFX_SURFACE_PROXY_SURFACE (mem->proxy);
    return mem->surface != NULL;
}


gboolean
gst_video_meta_map_mfx_surface (GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags)
{
    GstMfxVideoMemory *const mem =
      GST_MFX_VIDEO_MEMORY_CAST (gst_buffer_peek_memory (meta->buffer, 0));

    g_return_val_if_fail (mem, FALSE);
    g_return_val_if_fail (GST_MFX_IS_VIDEO_ALLOCATOR (mem->parent_instance.
        allocator), FALSE);
    g_return_val_if_fail (mem->meta, FALSE);

    /* Map for writing */
    if (++mem->map_count == 1) {
        if (!ensure_surface (mem))
            goto error_ensure_surface;
        if (!ensure_image (mem))
            goto error_ensure_image;

        // Load VA image from surface
        if ((flags & GST_MAP_READ))
            goto error_no_current_image;

        if (!gst_vaapi_image_map (mem->image))
            goto error_map_image;
    }

    *data = gst_vaapi_image_get_plane (mem->image, plane);
    *stride = gst_vaapi_image_get_pitch (mem->image, plane);
    info->flags = flags;
    return TRUE;

  /* ERRORS */
error_ensure_surface:
    {
        const GstVideoInfo *const vip = mem->surface_info;
        GST_ERROR ("failed to create NV12 surface of size %ux%u",
            GST_VIDEO_INFO_WIDTH (vip), GST_VIDEO_INFO_HEIGHT (vip));
        return FALSE;
    }
error_ensure_image:
  {
    const GstVideoInfo *const vip = mem->surface_info;
    GST_ERROR ("failed to create NV12 image of size %ux%u",
        GST_VIDEO_INFO_WIDTH (vip), GST_VIDEO_INFO_HEIGHT (vip));
    return FALSE;
  }
error_map_image:
    {
        GST_ERROR ("failed to map image");
        return FALSE;
    }
error_no_current_image:
    {
        GST_ERROR ("failed to make image current");
        return FALSE;
    }

    return TRUE;
}

gboolean
gst_video_meta_unmap_mfx_surface (GstVideoMeta * meta, guint plane,
    GstMapInfo * info)
{

    GstMfxVideoMemory *const mem =
        GST_MFX_VIDEO_MEMORY_CAST (gst_buffer_peek_memory (meta->buffer, 0));

    g_return_val_if_fail (mem, FALSE);
    g_return_val_if_fail (GST_MFX_IS_VIDEO_ALLOCATOR (mem->parent_instance.
        allocator), FALSE);
    g_return_val_if_fail (mem->meta, FALSE);
    g_return_val_if_fail (mem->surface, FALSE);
    g_return_val_if_fail (mem->image, FALSE);

    if (--mem->map_count == 0) {
    /* Unmap VA image used for read/writes */
        if (info->flags & GST_MAP_READWRITE) {
            gst_vaapi_image_unmap (mem->image);
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

	vip = &allocator->surface_info;
	gst_memory_init(&mem->parent_instance, GST_MEMORY_FLAG_NO_SHARE,
		gst_object_ref(allocator), NULL, GST_VIDEO_INFO_SIZE(vip), 0,
		0, GST_VIDEO_INFO_SIZE(vip));

	mem->proxy = NULL;
	mem->surface_info = &allocator->surface_info;
	mem->surface = NULL;
	mem->image = NULL;
	mem->meta = meta ? gst_mfx_video_meta_ref(meta) : NULL;
	//mem->map_type = 0;
	mem->map_count = 0;

	return GST_MEMORY_CAST(mem);
}


gst_mfx_video_memory_free (GstMfxVideoMemory * mem)
{
    mem->surface = NULL;
    gst_mfx_video_memory_reset_image (mem);
    gst_mfx_surface_proxy_replace (&mem->proxy, NULL);
    gst_mfx_video_meta_replace (&mem->meta, NULL);
    gst_object_unref (GST_MEMORY_CAST (mem)->allocator);
    g_slice_free (GstMfxVideoMemory, mem);
}

void
gst_mfx_video_memory_reset_image (GstMfxVideoMemory * mem)
{
    if (mem->image) {
        mem->image = NULL;
    }
}

void
gst_mfx_video_memory_reset_surface (GstMfxVideoMemory * mem)
{
    mem->surface = NULL;
    gst_mfx_video_memory_reset_image (mem);
    gst_mfx_surface_proxy_replace (&mem->proxy, NULL);
    if (mem->meta)
        gst_mfx_video_meta_set_surface_proxy (mem->meta, NULL);
}

gboolean
gst_vaapi_video_memory_sync (GstMfxVideoMemory * mem)
{
  g_return_val_if_fail (mem, FALSE);

  return ensure_surface_is_current (mem);
}

static gpointer
gst_vaapi_video_memory_map (GstVaapiVideoMemory * mem, gsize maxsize,
    guint flags)
{
  gpointer data;

  g_return_val_if_fail (mem, NULL);
  g_return_val_if_fail (mem->meta, NULL);

  if (mem->map_count == 0) {
    switch (flags & GST_MAP_READWRITE) {
      case 0:
        // No flags set: return a GstVaapiSurfaceProxy
        gst_vaapi_surface_proxy_replace (&mem->proxy,
            gst_vaapi_video_meta_get_surface_proxy (mem->meta));
        if (!mem->proxy)
          goto error_no_surface_proxy;
        if (!ensure_surface_is_current (mem))
          goto error_no_current_surface;
        mem->map_type = GST_VAAPI_VIDEO_MEMORY_MAP_TYPE_SURFACE;
        break;
      case GST_MAP_READ:
        // Only read flag set: return raw pixels
        if (!ensure_surface (mem))
          goto error_no_surface;
        if (!ensure_image (mem))
          goto error_no_image;
        if (!ensure_image_is_current (mem))
          goto error_no_current_image;
        if (!gst_vaapi_image_map (mem->image))
          goto error_map_image;
        mem->map_type = GST_VAAPI_VIDEO_MEMORY_MAP_TYPE_LINEAR;
        break;
      default:
        goto error_unsupported_map;
    }
  }

  switch (mem->map_type) {
    case GST_VAAPI_VIDEO_MEMORY_MAP_TYPE_SURFACE:
      if (!mem->proxy)
        goto error_no_surface_proxy;
      data = mem->proxy;
      break;
    case GST_VAAPI_VIDEO_MEMORY_MAP_TYPE_LINEAR:
      if (!mem->image)
        goto error_no_image;
      data = get_image_data (mem->image);
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
  GST_ERROR ("failed to extract GstVaapiSurfaceProxy from video meta");
  return NULL;
error_no_surface:
  GST_ERROR ("failed to extract VA surface from video buffer");
  return NULL;
error_no_current_surface:
  GST_ERROR ("failed to make surface current");
  return NULL;
error_no_image:
  GST_ERROR ("failed to extract VA image from video buffer");
  return NULL;
error_no_current_image:
  GST_ERROR ("failed to make image current");
  return NULL;
error_map_image:
  GST_ERROR ("failed to map VA image");
  return NULL;
}

static void
gst_vaapi_video_memory_unmap (GstVaapiVideoMemory * mem)
{
  if (mem->map_count == 1) {
    switch (mem->map_type) {
      case GST_VAAPI_VIDEO_MEMORY_MAP_TYPE_SURFACE:
        gst_vaapi_surface_proxy_replace (&mem->proxy, NULL);
        break;
      case GST_VAAPI_VIDEO_MEMORY_MAP_TYPE_LINEAR:
        gst_vaapi_image_unmap (mem->image);
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

static GstVaapiVideoMemory *
gst_vaapi_video_memory_copy (GstVaapiVideoMemory * mem,
    gssize offset, gssize size)
{
  GstVaapiVideoMeta *meta;
  GstMemory *out_mem;
  gsize maxsize;

  g_return_val_if_fail (mem, NULL);
  g_return_val_if_fail (mem->meta, NULL);

  /* XXX: this implements a soft-copy, i.e. underlying VA surfaces
     are not copied */
  (void) gst_memory_get_sizes (GST_MEMORY_CAST (mem), NULL, &maxsize);
  if (offset != 0 || (size != -1 && (gsize) size != maxsize))
    goto error_unsupported;

  if (!ensure_surface_is_current (mem))
    goto error_no_current_surface;

  meta = gst_vaapi_video_meta_copy (mem->meta);
  if (!meta)
    goto error_allocate_memory;

  out_mem = gst_vaapi_video_memory_new (GST_MEMORY_CAST (mem)->allocator, meta);
  gst_vaapi_video_meta_unref (meta);
  if (!out_mem)
    goto error_allocate_memory;
  return GST_VAAPI_VIDEO_MEMORY_CAST (out_mem);

  /* ERRORS */
error_no_current_surface:
  GST_ERROR ("failed to make surface current");
  return NULL;
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

    gst_mfx_object_pool_replace (&allocator->surface_pool, NULL);

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
gst_video_info_update_from_image (GstVideoInfo * vip, GstMfxVaapiImage * image)
{
    GstVideoFormat format;
    const guchar *data;
    guint i, num_planes, data_size, width, height;

    /* Reset format from image */
    format = gst_mfx_vaapi_image_get_format (image);
    gst_mfx_vaapi_image_get_size (image, &width, &height);
    gst_video_info_set_format (vip, format, width, height);

    num_planes = gst_mfx_image_get_plane_count (image);
    g_return_val_if_fail (num_planes == GST_VIDEO_INFO_N_PLANES (vip), FALSE);

    /* Determine the base data pointer */
    data = get_image_data (image);
    g_return_val_if_fail (data != NULL, FALSE);
    data_size = gst_mfx_vaapi_image_get_data_size (image);

    /* Check that we don't have disjoint planes */
    for (i = 0; i < num_planes; i++) {
        const guchar *const plane = gst_mfx_vaapi_image_get_plane (image, i);
        if (plane - data > data_size)
            return FALSE;
    }

    /* Update GstVideoInfo structure */
    for (i = 0; i < num_planes; i++) {
        const guchar *const plane = gst_mfx_image_get_plane (image, i);
        GST_VIDEO_INFO_PLANE_OFFSET (vip, i) = plane - data;
        GST_VIDEO_INFO_PLANE_STRIDE (vip, i) = gst_mfx_vaapi_image_get_pitch (image, i);
    }
    GST_VIDEO_INFO_SIZE (vip) = data_size;
    return TRUE;
}

static inline void
allocator_configure_surface_info (GstMfxVideoAllocator * allocator)
{
    const GstVideoInfo *vinfo;
    GstMfxSurface *surface = NULL;
    GstMfxVaapiImage *image = NULL;
    gboolean updated;
    GstVideoFormat fmt;

    vinfo = &allocator->video_info;

    fmt = gst_mfx_video_format_get_best_native (GST_VIDEO_INFO_FORMAT (vinfo));
    gst_video_info_set_format (&allocator->surface_info, fmt,
      GST_VIDEO_INFO_WIDTH (vinfo), GST_VIDEO_INFO_HEIGHT (vinfo));

    /* nothing to configure */
    if (USE_NATIVE_FORMATS ||
        GST_VIDEO_INFO_FORMAT (vinfo) == GST_VIDEO_FORMAT_ENCODED)
    return;

    surface = new_surface (vinfo);
    if (!surface)
        goto bail;
    image = gst_mfx_surface_derive_image (surface);
    if (!image)
        goto bail;
    if (!gst_mfx_vaapi_image_map (image))
        goto bail;

    updated = gst_video_info_update_from_image (&allocator->surface_info, image);

    gst_mfx_vaapi_image_unmap (image);

bail:
    if (surface)
        gst_mfx_mini_object_unref (surface);
    if (image)
        gst_mfx_mini_object_unref (image);
}

GstAllocator *
gst_mfx_video_allocator_new(const GstVideoInfo * vip)
{
	GstMfxVideoAllocator *allocator;

	g_return_val_if_fail(vip != NULL, NULL);

	allocator = g_object_new(GST_MFX_TYPE_VIDEO_ALLOCATOR, NULL);
	if (!allocator)
		return NULL;

	allocator->video_info = *vip;

	allocator_configure_surface_info(allocator);

	allocator->surface_pool = gst_mfx_surface_pool_new(vip);

	if (!allocator->surface_pool)
		goto error_create_surface_pool;

	gst_allocator_set_mfx_video_info(GST_ALLOCATOR_CAST(allocator),
		&allocator->surface_info, 0);
	return GST_ALLOCATOR_CAST(allocator);

	/* ERRORS */
error_create_surface_pool:
	{
		GST_ERROR("failed to allocate MFX surface pool");
		gst_object_unref(allocator);
		return NULL;
	}
}

#endif

