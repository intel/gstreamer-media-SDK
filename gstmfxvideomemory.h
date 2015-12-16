#ifndef GST_MFX_VIDEO_MEMORY_H
#define GST_MFX_VIDEO_MEMORY_H

#include <gst/gstallocator.h>
#include <gst/video/video-info.h>
#include "gstmfxsurfaceproxy.h"
#include "gstvaapiimage.h"
#include "gstmfxvideometa.h"
#include "gstmfxobjectpool.h"
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
	GstMfxSurface *surface;
	const GstVideoInfo *image_info;
	GstVaapiImage *image;
	GstMfxVideoMeta *meta;
	//guint map_type;
	gint map_count;
	//gboolean use_direct_rendering;
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

gboolean
gst_mfx_video_memory_sync (GstMfxVideoMemory * mem);

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
	GstVideoInfo surface_info;
	GstMfxObjectPool *surface_pool;
	GstVideoInfo image_info;
	GstMfxObjectPool *image_pool;

	//gboolean has_direct_rendering;
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

/*GstAllocator *
gst_mfx_video_allocator_new(GstMfxDisplay * display,
	const GstVideoInfo * vip, guint flags);*/

GstAllocator *
gst_mfx_video_allocator_new(VaapiAllocatorContext * alloc_context);

const GstVideoInfo *
gst_allocator_get_mfx_video_info(GstAllocator * allocator,
	guint * out_flags_ptr);

gboolean
gst_allocator_set_mfx_video_info(GstAllocator * allocator,
	const GstVideoInfo * vip, guint flags);

G_END_DECLS

#endif /* GST_MFX_VIDEO_MEMORY_H */
