#include "gstmfxsurface.h"
#include "gstmfxsurface_priv.h"

// MSDK Helper macro definitions
#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))

static void
gst_mfx_surface_finalize(GstMfxSurface * surface)
{
    g_async_queue_push(surface->alloc_ctx->surface_queue,
        surface->surface->Data.MemId);
	g_slice_free(mfxFrameSurface1, surface->surface);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_class(void)
{
	static const GstMfxMiniObjectClass GstMfxSurfaceClass = {
		sizeof(GstMfxSurface),
		(GDestroyNotify)gst_mfx_surface_finalize
	};
	return &GstMfxSurfaceClass;
}

static gboolean
gst_mfx_surface_create(GstMfxSurface * surface, VaapiAllocatorContext *ctx)
{
    surface->alloc_ctx = ctx;
    surface->display = ctx->va_dpy;
	surface->format = gst_mfx_video_format_from_mfx_fourcc(ctx->frame_info.FourCC);
	surface->width = ctx->frame_info.Width;
	surface->height = ctx->frame_info.Height;

	surface->surface = g_slice_new0(mfxFrameSurface1);

	surface->surface->Data.MemId = g_async_queue_try_pop(ctx->surface_queue);

	memcpy(&(surface->surface->Info), &ctx->frame_info, sizeof(mfxFrameInfo));

	return TRUE;
}

GstMfxSurface *
gst_mfx_surface_new(VaapiAllocatorContext *ctx)
{
	GstMfxSurface *surface;

	surface = gst_mfx_mini_object_new(gst_mfx_surface_class());
	if (!surface)
		return NULL;

	if (!gst_mfx_surface_create(surface, ctx))
		goto error;
	return surface;

error:
	gst_mfx_mini_object_unref(surface);
	return NULL;
}

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface(GstMfxSurface * surface)
{
	g_return_val_if_fail(surface != NULL, NULL);

	return GST_MFX_SURFACE_FRAME_SURFACE(surface);
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
    g_return_val_if_fail (surface != NULL, NULL);

    mfxFrameSurface1 *surf = gst_mfx_surface_get_frame_surface(surface);
    if (!surf)
        return NULL;

    VAImage va_image;
    VAStatus status;
    VASurfaceID va_surface = *(VASurfaceID*)surf->Data.MemId;

    va_image.image_id = VA_INVALID_ID;
    va_image.buf = VA_INVALID_ID;

    status = vaDeriveImage (surface->display, va_surface, &va_image);
    if (status != VA_STATUS_SUCCESS)
        goto error;

    if (va_image.image_id == VA_INVALID_ID || va_image.buf == VA_INVALID_ID)
        return NULL;

    return gst_vaapi_image_new_with_image(surface->display, &va_image);

error:
    GST_ERROR ("Error deriving image: %s\n",
        vaErrorStr(status));
    return NULL;
}


VAImage *
gst_mfx_surface_get_image (GstMfxSurface * surface)
{
    g_return_val_if_fail (surface != NULL, NULL);

    mfxFrameSurface1 *surf = gst_mfx_surface_get_frame_surface(surface);
    if (!surf)
        return NULL;

    int va_width = MSDK_ALIGN32(surf->Info.Width);
    int va_height = MSDK_ALIGN32(surf->Info.Height);

    VAImage va_image;
    VAStatus status;
    VASurfaceID va_surface = *(VASurfaceID*)surf->Data.MemId;
    VAImageFormat img_fmt = {
        .fourcc         = VA_FOURCC_NV12,
        .byte_order     = VA_LSB_FIRST,
        .bits_per_pixel = 8,
        .depth          = 8,
    };

    va_image.image_id = VA_INVALID_ID;
    va_image.buf = VA_INVALID_ID;

    status = vaCreateImage(surface->display, &img_fmt,
                va_width, va_height, &va_image);
    if (status != VA_STATUS_SUCCESS)
        goto error;

    status = vaGetImage(surface->display, va_surface, 0, 0,
                va_width, va_height,
                va_image.image_id);
    if (status != VA_STATUS_SUCCESS)
        goto error;

    return &va_image;

error:
    GST_ERROR ("Error getting an image: %s\n",
        vaErrorStr(status));
    return NULL;
}
