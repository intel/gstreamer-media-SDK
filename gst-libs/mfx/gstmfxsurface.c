#include "sysdeps.h"
#include "gstmfxsurface.h"
#include "gstmfxsurface_priv.h"
#include "gstvaapiimage.h"
#include "gstvaapiimage_priv.h"
#include "gstvaapibufferproxy_priv.h"

static void
gst_mfx_surface_destroy(GstMfxSurface * surface)
{
    g_async_queue_push(surface->alloc_ctx->surface_queue,
		GST_MFX_OBJECT_ID(surface));
	g_slice_free(mfxFrameSurface1, surface->surface);
	GST_MFX_OBJECT_ID(surface) = VA_INVALID_SURFACE;
}

static gboolean
gst_mfx_surface_create(GstMfxSurface * surface, GstMfxContextAllocatorVaapi *ctx)
{
	VASurfaceID *surface_id;

    surface->alloc_ctx = ctx;
	surface->format = gst_mfx_video_format_from_mfx_fourcc(ctx->frame_info.FourCC);
	surface->width = ctx->frame_info.Width;
	surface->height = ctx->frame_info.Height;

	surface_id = g_async_queue_try_pop(ctx->surface_queue);
	if (!surface_id)
		return FALSE;

	GST_DEBUG("surface %" GST_MFX_ID_FORMAT, GST_MFX_ID_ARGS(surface_id));
	GST_MFX_OBJECT_ID(surface) = *surface_id;

	surface->surface = g_slice_new0(mfxFrameSurface1);
	if (!surface->surface)
		return FALSE;

	surface->surface->Data.MemId = surface_id;
	memcpy(&(surface->surface->Info), &ctx->frame_info, sizeof(mfxFrameInfo));

	return TRUE;
}

#define gst_mfx_surface_finalize gst_mfx_surface_destroy
GST_MFX_OBJECT_DEFINE_CLASS(GstMfxSurface, gst_mfx_surface);

GstMfxSurface *
gst_mfx_surface_new(GstMfxDisplay * display, GstMfxContextAllocatorVaapi *ctx)
{
	GstMfxSurface *surface;

	surface = gst_mfx_object_new(gst_mfx_surface_class(), display);
	if (!surface)
		return NULL;

	if (!gst_mfx_surface_create(surface, ctx))
		goto error;
	return surface;

error:
	gst_mfx_object_unref(surface);
	return NULL;
}

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface(GstMfxSurface * surface)
{
	g_return_val_if_fail(surface != NULL, NULL);

	return GST_MFX_SURFACE_FRAME_SURFACE(surface);
}


/**
* gst_mfx_surface_get_id:
* @surface: a #GstMFxSurface
*
* Returns the underlying VASurfaceID of the @surface.
*
* Return value: the underlying VA surface id
*/
GstMfxID
gst_mfx_surface_get_id(GstMfxSurface * surface)
{
	g_return_val_if_fail(surface != NULL, VA_INVALID_SURFACE);

	return GST_MFX_OBJECT_ID(surface);
}

/**
* gst_mfx_surface_get_format:
* @surface: a #GstMfxSurface
*
* Returns the #GstVideoFormat the @surface was created with.
*
* Return value: the #GstVideoFormat, or %GST_VIDEO_FORMAT_ENCODED if
*   the surface was not created with an explicit video format, or if
*   the underlying video format could not be determined
*/
GstVideoFormat
gst_mfx_surface_get_format(GstMfxSurface * surface)
{
	g_return_val_if_fail(surface != NULL, 0);

	return GST_MFX_SURFACE_FORMAT(surface);
}

/**
* gst_mfx_surface_get_width:
* @surface: a #GstMfxSurface
*
* Returns the @surface width.
*
* Return value: the surface width, in pixels
*/
guint
gst_mfx_surface_get_width(GstMfxSurface * surface)
{
	g_return_val_if_fail(surface != NULL, 0);

	return GST_MFX_SURFACE_WIDTH(surface);
}

/**
* gst_mfx_surface_get_height:
* @surface: a #GstMfxSurface
*
* Returns the @surface height.
*
* Return value: the surface height, in pixels.
*/
guint
gst_mfx_surface_get_height(GstMfxSurface * surface)
{
	g_return_val_if_fail(surface != NULL, 0);

	return GST_MFX_SURFACE_HEIGHT(surface);
}

/**
* gst_mfx_surface_get_size:
* @surface: a #GstMfxSurface
* @width_ptr: return location for the width, or %NULL
* @height_ptr: return location for the height, or %NULL
*
* Retrieves the dimensions of a #GstMfxSurface.
*/
void
gst_mfx_surface_get_size(GstMfxSurface * surface,
	guint * width_ptr, guint * height_ptr)
{
	g_return_if_fail(surface != NULL);

	if (width_ptr)
		*width_ptr = GST_MFX_SURFACE_WIDTH(surface);

	if (height_ptr)
		*height_ptr = GST_MFX_SURFACE_HEIGHT(surface);
}


GstVaapiImage *
gst_mfx_surface_derive_image (GstMfxSurface * surface)
{
	GstMfxDisplay *display;
	VAImage va_image;
	VAStatus status;

    g_return_val_if_fail (surface != NULL, NULL);

	display = GST_MFX_OBJECT_DISPLAY(surface);
    va_image.image_id = VA_INVALID_ID;
    va_image.buf = VA_INVALID_ID;

	GST_MFX_DISPLAY_LOCK(display);
	status = vaDeriveImage(GST_MFX_DISPLAY_VADISPLAY(display),
		GST_MFX_OBJECT_ID(surface), &va_image);
	GST_MFX_DISPLAY_UNLOCK(display);
	if (!vaapi_check_status(status, "vaDeriveImage()")) {
		return NULL;
	}
    if (va_image.image_id == VA_INVALID_ID || va_image.buf == VA_INVALID_ID)
        return NULL;

    return gst_vaapi_image_new_with_image(display, &va_image);
}
