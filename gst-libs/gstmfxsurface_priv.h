#ifndef GST_MFX_SURFACE_PRIV_H
#define GST_MFX_SURFACE_PRIV_H

#include "gstmfxsurface.h"
#include "gstmfxobject_priv.h"

G_BEGIN_DECLS

typedef struct _GstMfxSurfaceClass GstMfxSurfaceClass;

/**
* GstMfxSurface:
*
* A mfxFrameSurface1 wrapper.
*/
struct _GstMfxSurface
{
	/*< private >*/
	GstMfxObject parent_instance;

	//GstMfxContext *parent_context;
	GstVideoFormat format;
	guint width;
	guint height;

	mfxFrameSurface1 *surface;
	GstMfxContextAllocatorVaapi *alloc_ctx;
};

struct _GstMfxSurfaceClass
{
	/*< private >*/
	GstMfxObjectClass parent_class;
};

/**
* GST_MFX_SURFACE_SURFACE_FORMAT:
* @surface: a #GstMfxSurface
*
* Macro that evaluates to the @surface format.
*
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_FORMAT
#define GST_MFX_SURFACE_FORMAT(surface) \
	(GST_MFX_SURFACE (surface)->format)

/**
* GST_MFX_SURFACE_SURFACE_WIDTH:
* @surface: a #GstMfxSurface
*
* Macro that evaluates to the @surface width in pixels.
*
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_WIDTH
#define GST_MFX_SURFACE_WIDTH(surface) \
	(GST_MFX_SURFACE (surface)->width)

/**
* GST_MFX_SURFACE_SURFACE_HEIGHT:
* @surface: a #GstMfxSurface
*
* Macro that evaluates to the @surface height in pixels.
*
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_SURFACE_HEIGHT
#define GST_MFX_SURFACE_HEIGHT(surface) \
	(GST_MFX_SURFACE (surface)->height)


#undef  GST_MFX_SURFACE_FRAME_SURFACE
#define GST_MFX_SURFACE_FRAME_SURFACE(surface) \
	(GST_MFX_SURFACE (surface)->surface)

#endif
