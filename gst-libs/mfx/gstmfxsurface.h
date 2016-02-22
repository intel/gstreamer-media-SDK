#ifndef GST_MFX_SURFACE_H
#define GST_MFX_SURFACE_H

#include <mfxvideo.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#include <va/va_drmcommon.h>

#include "gstmfxobject.h"
#include "gstmfxdisplay.h"
#include "gstvaapiimage.h"
#include "video-utils.h"
#include "gstmfxcontext.h"

G_BEGIN_DECLS

/**
* GST_MFX_SURFACE_CAPS_NAME:
*
* Generic caps type for VA surfaces.
*/
#define GST_MFX_SURFACE_CAPS_NAME "video/x-surface"

/**
* GST_MFX_SURFACE_CAPS:
*
* Generic caps for VA surfaces.
*/
#define GST_MFX_SURFACE_CAPS                  \
	GST_MFX_SURFACE_CAPS_NAME ", "            \
	"type = mfx, "                            \
	"opengl = (boolean) { true, false }, "      \
	"width  = (int) [ 1, MAX ], "               \
	"height = (int) [ 1, MAX ], "               \
	"framerate = (fraction) [ 0, MAX ]"

#define GST_MFX_SURFACE(obj) \
	((GstMfxSurface *)(obj))

typedef struct _GstMfxSurface GstMfxSurface;
typedef struct _GstMfxSurfaceProxy GstMfxSurfaceProxy;

GstMfxSurface *
gst_mfx_surface_new(GstMfxDisplay * display, GstMfxContextAllocatorVaapi *ctx);

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface(GstMfxSurface * surface);

GstMfxID
gst_mfx_surface_get_id(GstMfxSurface * surface);

GstVideoFormat
gst_mfx_surface_get_format(GstMfxSurface * surface);

guint
gst_mfx_surface_get_width(GstMfxSurface * surface);

guint
gst_mfx_surface_get_height(GstMfxSurface * surface);

void
gst_mfx_surface_get_size(GstMfxSurface * surface, guint * width_ptr,
	guint * height_ptr);

GstVaapiImage *
gst_mfx_surface_derive_image (GstMfxSurface * surface);

G_END_DECLS

#endif /* GST_MFX_SURFACE_H */
