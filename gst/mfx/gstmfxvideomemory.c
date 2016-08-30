/*
 *  Copyright (C) 2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "gstmfxvideomemory.h"

GST_DEBUG_CATEGORY_STATIC (gst_debug_mfxvideomemory);
#define GST_CAT_DEFAULT gst_debug_mfxvideomemory

static void gst_mfx_video_memory_reset_image (GstMfxVideoMemory * mem);

static gboolean
copy_image (GstMfxVideoMemory * mem)
{
  guint i, j, src_stride, dest_stride, height, offset, num_planes, plane_size;
  guint8 *src_plane = NULL;

  guint data_size = GST_VIDEO_INFO_SIZE (mem->image_info);
  mem->data = (guint8 *) g_slice_alloc0 (data_size);
  if (!mem->data)
    return FALSE;

  num_planes = GST_VIDEO_INFO_N_PLANES (mem->image_info);

  for (i = 0; i < num_planes; i++) {
    if (mem->image) {
      src_plane = vaapi_image_get_plane (mem->image, i);
      src_stride = vaapi_image_get_pitch (mem->image, i);
    } else {
      src_plane = gst_mfx_surface_proxy_get_plane (mem->proxy, i);
      src_stride = gst_mfx_surface_proxy_get_pitch (mem->proxy, i);
    }

    dest_stride = GST_VIDEO_INFO_PLANE_STRIDE (mem->image_info, i);
    offset = GST_VIDEO_INFO_PLANE_OFFSET (mem->image_info, i);

    if (i != num_planes - 1)
      plane_size =
          GST_VIDEO_INFO_PLANE_OFFSET (mem->image_info, i + 1) - offset;
    else
      plane_size = data_size - offset;

    if (src_stride != dest_stride) {
      height = plane_size / dest_stride;
      for (j = 0; j < height; j++) {
        memcpy (mem->data + offset, src_plane, dest_stride);
        src_plane += src_stride;
        offset += dest_stride;
      }
    } else
      memcpy (mem->data + offset, src_plane, plane_size);
  }

  mem->mapped = FALSE;

  return TRUE;
}

static gboolean
get_image_data (GstMfxVideoMemory * mem)
{
  guint width = GST_VIDEO_INFO_WIDTH (mem->image_info);
  guint aligned_width = GST_MFX_SURFACE_PROXY_WIDTH (mem->proxy);
  guint height = GST_VIDEO_INFO_HEIGHT (mem->image_info);
  guint aligned_height = GST_MFX_SURFACE_PROXY_HEIGHT (mem->proxy);

  if ((width == aligned_width && height == aligned_height && !mem->image) ||
      GST_VIDEO_INFO_N_PLANES (mem->image_info) == 1) {
    if (mem->image)
      mem->data = vaapi_image_get_plane (mem->image, 0);
    else
      mem->data = gst_mfx_surface_proxy_get_plane (mem->proxy, 0);
    mem->mapped = TRUE;
    return TRUE;
  } else {
    return copy_image (mem);
  }
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
  }

  if (!mem->proxy) {
    mem->proxy = new_surface_proxy (mem);
    if (!mem->proxy)
      return FALSE;
    gst_mfx_video_meta_set_surface_proxy (mem->meta, mem->proxy);
  }

  return TRUE;
}

gboolean
gst_video_meta_map_mfx_surface (GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags)
{
  GstMfxVideoMemory *const mem =
      GST_MFX_VIDEO_MEMORY_CAST (gst_buffer_peek_memory (meta->buffer, 0));

  g_return_val_if_fail (mem, FALSE);
  g_return_val_if_fail (GST_MFX_IS_VIDEO_ALLOCATOR (mem->
          parent_instance.allocator), FALSE);
  g_return_val_if_fail (mem->meta, FALSE);

  /* Map for writing */
  if (++mem->map_count == 1) {
    if (!ensure_surface (mem))
      goto error_ensure_surface;

    if (!gst_mfx_surface_proxy_is_mapped (mem->proxy)) {
      if (!ensure_image (mem))
        goto error_ensure_image;

      // Load VA image from surface
      if (!vaapi_image_map (mem->image))
        goto error_map_image;
    }
  }

  if (!mem->image) {
    *data = gst_mfx_surface_proxy_get_plane (mem->proxy, plane);
    *stride = gst_mfx_surface_proxy_get_pitch (mem->proxy, plane);
  } else {
    *data = vaapi_image_get_plane (mem->image, plane);
    *stride = vaapi_image_get_pitch (mem->image, plane);
  }

  info->flags = flags;
  return TRUE;

  /* ERRORS */
error_ensure_surface:
  {
    const GstVideoInfo *const vip = mem->image_info;
    GST_ERROR ("failed to create surface of size %ux%u",
        GST_VIDEO_INFO_WIDTH (vip), GST_VIDEO_INFO_HEIGHT (vip));
    return FALSE;
  }
error_ensure_image:
  {
    const GstVideoInfo *const vip = mem->image_info;
    GST_ERROR ("failed to create image of size %ux%u",
        GST_VIDEO_INFO_WIDTH (vip), GST_VIDEO_INFO_HEIGHT (vip));
    return FALSE;
  }
error_map_image:
  {
    GST_ERROR ("failed to map image");
    return FALSE;
  }
}

gboolean
gst_video_meta_unmap_mfx_surface (GstVideoMeta * meta, guint plane,
    GstMapInfo * info)
{
  GstMfxVideoMemory *const mem =
      GST_MFX_VIDEO_MEMORY_CAST (gst_buffer_peek_memory (meta->buffer, 0));

  g_return_val_if_fail (mem, FALSE);
  g_return_val_if_fail (GST_MFX_IS_VIDEO_ALLOCATOR (mem->
          parent_instance.allocator), FALSE);
  g_return_val_if_fail (mem->meta, FALSE);
  g_return_val_if_fail (mem->proxy, FALSE);

  if (--mem->map_count == 0) {
    /* Unmap VA image used for read/writes */
    if (mem->image && info->flags & GST_MAP_READWRITE) {
      vaapi_image_unmap (mem->image);
    }
  }

  return TRUE;
}

GstMemory *
gst_mfx_video_memory_new (GstAllocator * base_allocator, GstMfxVideoMeta * meta)
{
  GstMfxVideoAllocator *const allocator =
      GST_MFX_VIDEO_ALLOCATOR_CAST (base_allocator);

  const GstVideoInfo *vip;
  GstMfxVideoMemory *mem;

  g_return_val_if_fail (GST_MFX_IS_VIDEO_ALLOCATOR (allocator), NULL);

  mem = g_slice_new (GstMfxVideoMemory);
  if (!mem)
    return NULL;

  vip = &allocator->image_info;
  gst_memory_init (&mem->parent_instance, GST_MEMORY_FLAG_NO_SHARE,
      gst_object_ref (allocator), NULL, GST_VIDEO_INFO_SIZE (vip), 0,
      0, GST_VIDEO_INFO_SIZE (vip));

  mem->proxy = NULL;
  mem->image_info = &allocator->image_info;
  mem->image = NULL;
  mem->meta = meta ? gst_mfx_video_meta_ref (meta) : NULL;
  mem->map_type = 0;
  mem->map_count = 0;
  mem->mapped = FALSE;

  return GST_MEMORY_CAST (mem);
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
    vaapi_image_replace (&mem->image, NULL);
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
gst_mfx_video_memory_map (GstMfxVideoMemory * mem, gsize maxsize, guint flags)
{
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
        if (!gst_mfx_surface_proxy_is_mapped (mem->proxy)) {
          if (!ensure_image (mem))
            goto error_no_image;
          if (!vaapi_image_map (mem->image))
            goto error_map_image;
          mem->map_type = GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR;
        } else
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
      mem->data = mem->proxy;
      break;
    case GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR:
      if (!mem->image)
        goto error_no_image;
    case GST_MFX_SYSTEM_MEMORY_MAP_TYPE_LINEAR:
      if (!get_image_data (mem))
        goto error_no_image;
      break;
    default:
      goto error_unsupported_map_type;
  }
  mem->map_count++;
  return mem->data;

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
      case GST_MFX_SYSTEM_MEMORY_MAP_TYPE_LINEAR:
        if (!mem->mapped)
          g_slice_free1 (GST_VIDEO_INFO_SIZE (mem->image_info), mem->data);
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
gst_mfx_video_memory_copy (GstMfxVideoMemory * mem, gssize offset, gssize size)
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
gst_mfx_video_memory_share (GstMfxVideoMemory * mem, gssize offset, gssize size)
{
  GST_FIXME ("unimplemented GstMfxVideoAllocator::mem_share () hook");
  return NULL;
}

static gboolean
gst_mfx_video_memory_is_span (GstMfxVideoMemory * mem1,
    GstMfxVideoMemory * mem2, gsize * offset_ptr)
{
  GST_FIXME ("unimplemented GstMfxVideoAllocator::mem_is_span () hook");
  return FALSE;
}

/* ------------------------------------------------------------------------ */
/* --- GstMfxVideoAllocator                                           --- */
/* ------------------------------------------------------------------------ */

#define GST_MFX_VIDEO_ALLOCATOR_CLASS (klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_MFX_TYPE_VIDEO_ALLOCATOR, \
        GstMfxVideoAllocatorClass))

#define GST_MFX_IS_VIDEO_ALLOCATOR_CLASS (klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_MFX_TYPE_VIDEO_ALLOCATOR))

G_DEFINE_TYPE (GstMfxVideoAllocator,
    gst_mfx_video_allocator, GST_TYPE_ALLOCATOR);

static GstMemory *
gst_mfx_video_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_warning ("use gst_mfx_video_memory_new () to allocate from "
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
  GstMfxVideoAllocator *const allocator = GST_MFX_VIDEO_ALLOCATOR_CAST (object);

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

GstAllocator *
gst_mfx_video_allocator_new (GstMfxDisplay * display,
    const GstVideoInfo * vip, gboolean mapped)
{
  GstMfxVideoAllocator *allocator;

  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (vip != NULL, NULL);

  allocator = g_object_new (GST_MFX_TYPE_VIDEO_ALLOCATOR, NULL);
  if (!allocator)
    return NULL;

  allocator->image_info = *vip;

  allocator->surface_pool = gst_mfx_surface_pool_new (display,
      &allocator->image_info, mapped);
  if (!allocator->surface_pool)
    goto error_create_surface_pool;

  return GST_ALLOCATOR_CAST (allocator);
  /* ERRORS */
error_create_surface_pool:
  {
    GST_ERROR ("failed to allocate MFX surface pool");
    gst_object_unref (allocator);
    return NULL;
  }
}

/* ------------------------------------------------------------------------ */
/* --- GstMfxDmaBufMemory                                             --- */
/* ------------------------------------------------------------------------ */

#define GST_MFX_PRIME_BUFFER_PROXY_QUARK gst_mfx_prime_buffer_proxy_quark_get ()
static GQuark
gst_mfx_prime_buffer_proxy_quark_get (void)
{
  static gsize g_quark;

  if (g_once_init_enter (&g_quark)) {
    gsize quark = (gsize) g_quark_from_static_string ("GstMfxPrimeBufferProxy");
    g_once_init_leave (&g_quark, quark);
  }
  return g_quark;
}

GstMemory *
gst_mfx_dmabuf_memory_new (GstAllocator * allocator, GstMfxDisplay * display,
    const GstVideoInfo * vip, GstMfxVideoMeta * meta)
{
  GstMemory *mem;
  GstMfxSurfaceProxy *proxy;
  GstMfxPrimeBufferProxy *dmabuf_proxy;
  gint dmabuf_fd;

  g_return_val_if_fail (allocator != NULL, NULL);
  g_return_val_if_fail (meta != NULL, NULL);

  proxy = gst_mfx_surface_proxy_new (display, vip, FALSE);
  if (!proxy)
    goto error_create_surface_proxy;

  dmabuf_proxy = gst_mfx_prime_buffer_proxy_new_from_surface (proxy);
  if (!dmabuf_proxy)
    goto error_create_dmabuf_proxy;

  gst_mfx_video_meta_set_surface_proxy (meta, proxy);
  gst_mfx_surface_proxy_unref (proxy);

  dmabuf_fd = gst_mfx_prime_buffer_proxy_get_handle (dmabuf_proxy);
  if (dmabuf_fd < 0 || (dmabuf_fd = dup (dmabuf_fd)) < 0)
    goto error_create_dmabuf_handle;

  mem = gst_dmabuf_allocator_alloc (allocator, dmabuf_fd,
      gst_mfx_prime_buffer_proxy_get_size (dmabuf_proxy));
  if (!mem)
    goto error_create_dmabuf_memory;

  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem),
      GST_MFX_PRIME_BUFFER_PROXY_QUARK, dmabuf_proxy,
      (GDestroyNotify) gst_mfx_prime_buffer_proxy_unref);

  return mem;
  /* ERRORS */
error_create_surface_proxy:
  {
    GST_ERROR ("failed to create VA surface (format:%s size:%ux%u)",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (vip)),
        GST_VIDEO_INFO_WIDTH (vip), GST_VIDEO_INFO_HEIGHT (vip));
    return NULL;
  }
error_create_dmabuf_proxy:
  {
    GST_ERROR ("failed to export MFX surface to DMABUF");
    gst_mfx_surface_proxy_unref (proxy);
    return NULL;
  }
error_create_dmabuf_handle:
  {
    GST_ERROR ("failed to duplicate DMABUF handle");
    gst_mfx_prime_buffer_proxy_unref (dmabuf_proxy);
    return NULL;
  }
error_create_dmabuf_memory:
  {
    GST_ERROR ("failed to create DMABUF memory");
    gst_mfx_prime_buffer_proxy_unref (dmabuf_proxy);
    return NULL;
  }
}
