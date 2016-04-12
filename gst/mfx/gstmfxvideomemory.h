#ifndef GST_MFX_VIDEO_MEMORY_H
#define GST_MFX_VIDEO_MEMORY_H

#include "sysdeps.h"
#include <gst/gstallocator.h>
#include <gst/video/video-info.h>
#include "gstmfxsurfaceproxy.h"
#include "gstmfxutils_vaapi.h"
#include "gstmfxvideometa.h"
#include "gstmfxdisplay.h"
#include "gstmfxtaskaggregator.h"
#include <gst/allocators/allocators.h>

G_BEGIN_DECLS

typedef struct _GstMfxVideoMemory GstMfxVideoMemory;
typedef struct _GstMfxVideoAllocator GstMfxVideoAllocator;
typedef struct _GstMfxVideoAllocatorClass GstMfxVideoAllocatorClass;

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
    GST_MFX_VIDEO_MEMORY_MAP_TYPE_LINEAR
} GstMfxVideoMemoryMapType;

/**
* GstMfxVideoMemory:
*
* A VA video memory object holder, including VA surfaces, images and
* proxies.
*/
struct _GstMfxVideoMemory
{
	GstMemory parent_instance;

	/*< private >*/
	GstMfxSurfaceProxy *proxy;
	const GstVideoInfo *surface_info;
	const GstVideoInfo *image_info;
	VaapiImage *image;
	GstMfxVideoMeta *meta;
	guint map_type;
	gint map_count;
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
	GstAllocator parent_instance;

	/*< private >*/
	GstVideoInfo video_info;
	GstVideoInfo image_info;
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
	const GstVideoInfo * vip);

const GstVideoInfo *
gst_allocator_get_mfx_video_info(GstAllocator * allocator,
	guint * out_flags_ptr);

gboolean
gst_allocator_set_mfx_video_info(GstAllocator * allocator,
	const GstVideoInfo * vip, guint flags);

G_END_DECLS

#endif /* GST_MFX_VIDEO_MEMORY_H */
