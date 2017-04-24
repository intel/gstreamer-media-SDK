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

#ifndef GST_MFX_VIDEO_MEMORY_H
#define GST_MFX_VIDEO_MEMORY_H

#include "gst-libs/mfx/sysdeps.h"
#include <gst/gstallocator.h>
#include <gst/video/video-info.h>
#include <gst/allocators/allocators.h>

#include "gstmfxvideometa.h"

#include <gst-libs/mfx/gstmfxdisplay.h>
#include <gst-libs/mfx/gstmfxtaskaggregator.h>
#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst-libs/mfx/gstmfxsurface_vaapi.h>
#include <gst-libs/mfx/gstmfxprimebufferproxy.h>
#include <gst-libs/mfx/gstmfxsurfacepool.h>
#include <gst-libs/mfx/gstmfxutils_vaapi.h>

G_BEGIN_DECLS

typedef struct _GstMfxVideoMemory           GstMfxVideoMemory;
typedef struct _GstMfxVideoAllocator        GstMfxVideoAllocator;
typedef struct _GstMfxVideoAllocatorClass   GstMfxVideoAllocatorClass;

/* ------------------------------------------------------------------------ */
/* --- GstMfxVideoMemory                                              --- */
/* ------------------------------------------------------------------------ */

#define GST_MFX_VIDEO_MEMORY_CAST(mem) \
  ((GstMfxVideoMemory *) (mem))

#define GST_MFX_IS_VIDEO_MEMORY(mem) \
  ((mem) && (mem)->allocator && GST_MFX_IS_VIDEO_ALLOCATOR((mem)->allocator))

#define GST_MFX_VIDEO_MEMORY_NAME             "GstMfxVideoMemory"

#define GST_CAPS_FEATURE_MEMORY_MFX_SURFACE   "memory:MFXSurface"

#define GST_MFX_VIDEO_MEMORY_FLAG_IS_SET(mem, flag) \
  GST_MEMORY_FLAG_IS_SET (mem, flag)
#define GST_MFX_VIDEO_MEMORY_FLAG_SET(mem, flag) \
  GST_MINI_OBJECT_FLAG_SET (mem, flag)
#define GST_MFX_VIDEO_MEMORY_FLAG_UNSET(mem, flag) \
  GST_MEMORY_FLAG_UNSET (mem, flag)

/**
 * GstMfxVideoMemoryMapType:
 * @GST_MFX_VIDEO_MEMORY_MAP_TYPE_SURFACE: map with gst_buffer_map()
 *   and flags = 0x00 to return a #GstMfxSurfaceProxy
 * @GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR: map with gst_buffer_map()
 *   and flags = GST_MAP_READ to return the raw pixels of the whole image
 *
 * The set of all #GstMfxVideoMemory map types.
 */
typedef enum
{
    GST_MFX_VIDEO_MEMORY_MAP_TYPE_SURFACE = 1,
    GST_MFX_SYSTEM_MEMORY_MAP_TYPE_LINEAR
} GstMfxVideoMemoryMapType;

/**
* GstMfxVideoMemory:
*
* A VA video memory object holder, including VA surfaces, images and
* proxies.
*/
struct _GstMfxVideoMemory
{
  GstMemory            parent_instance;

  /*< private >*/
  GstMfxSurface       *surface;
  const GstVideoInfo  *image_info;
  VaapiImage          *image;
  GstMfxVideoMeta     *meta;
  guint                map_type;
  guint8              *data;
  gboolean             new_copy;
};

GstMemory *
gst_mfx_video_memory_new (GstAllocator * allocator, GstMfxVideoMeta * meta);

gboolean
gst_video_meta_map_mfx_surface (GstVideoMeta * meta, guint plane,
    GstMapInfo * info, gpointer * data, gint * stride, GstMapFlags flags);

gboolean
gst_video_meta_unmap_mfx_surface (GstVideoMeta * meta, guint plane,
    GstMapInfo * info);

void
gst_mfx_video_memory_reset_surface (GstMfxVideoMemory * mem);


/* ------------------------------------------------------------------------ */
/* --- GstMfxVideoAllocator                                           --- */
/* ------------------------------------------------------------------------ */

#define GST_MFX_VIDEO_ALLOCATOR_CAST(allocator) \
  ((GstMfxVideoAllocator *) (allocator))

#define GST_MFX_TYPE_VIDEO_ALLOCATOR \
  (gst_mfx_video_allocator_get_type ())
#define GST_MFX_VIDEO_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_MFX_TYPE_VIDEO_ALLOCATOR, \
  GstMfxVideoAllocator))
#define GST_MFX_IS_VIDEO_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_MFX_TYPE_VIDEO_ALLOCATOR))

#define GST_MFX_VIDEO_ALLOCATOR_NAME          "GstMfxVideoAllocator"

/**
* GstMfxVideoAllocator:
*
* A VA video memory allocator object.
*/
struct _GstMfxVideoAllocator
{
  GstAllocator         parent_instance;

  /*< private >*/
  GstVideoInfo         image_info;
  GstMfxSurfacePool   *surface_pool;
};

/**
* GstMfxVideoAllocatorClass:
*
* A VA video memory allocator class.
*/
struct _GstMfxVideoAllocatorClass
{
  GstAllocatorClass parent_class;
};

GType
gst_mfx_video_allocator_get_type(void);

GstAllocator *
gst_mfx_video_allocator_new(GstMfxDisplay * display,
    const GstVideoInfo * vip, gboolean mapped);

/* ------------------------------------------------------------------------ */
/* --- GstMfxDmaBufMemory                                               --- */
/* ------------------------------------------------------------------------ */

GstMemory *
gst_mfx_dmabuf_memory_new(GstAllocator * allocator, GstMfxDisplay * display,
    const GstVideoInfo *vip, GstMfxVideoMeta * meta);

G_END_DECLS

#endif /* GST_MFX_VIDEO_MEMORY_H */
