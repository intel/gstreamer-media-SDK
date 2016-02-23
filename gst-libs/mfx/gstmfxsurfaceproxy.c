#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurfacepool.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfaceProxy
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxContextAllocator *ctx;
	GstMfxSurfacePool *pool;

	mfxFrameSurface1 *surface;
	GstVideoFormat format;
	guint width;
	guint height;
	GstMfxRectangle crop_rect;
};

static gboolean
mfx_surface_create(GstMfxSurfaceProxy * proxy)
{
	mfxMemId surface_id;

	surface_id = g_async_queue_try_pop(proxy->ctx->surface_queue);
	if (!surface_id)
		return FALSE;

	proxy->surface = g_slice_new0(mfxFrameSurface1);
	if (!proxy->surface)
		return FALSE;

	proxy->surface->Data.MemId = surface_id;
	memcpy(&proxy->surface->Info, &proxy->ctx->frame_info,
		sizeof(mfxFrameInfo));

	return TRUE;
}

static void
gst_mfx_surface_proxy_finalize(GstMfxSurfaceProxy * proxy)
{
	if (proxy->surface) {
		if (proxy->pool)
			gst_mfx_object_pool_put_object(proxy->pool, proxy);

		g_async_queue_push(proxy->ctx->surface_queue,
			GST_MFX_SURFACE_PROXY_MEMID(proxy));
		g_slice_free(mfxFrameSurface1, proxy->surface);
	}
	gst_mfx_object_pool_replace(&proxy->pool, NULL);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_proxy_class(void)
{
	static const GstMfxMiniObjectClass GstMfxSurfaceProxyClass = {
		sizeof (GstMfxSurfaceProxy),
		(GDestroyNotify)gst_mfx_surface_proxy_finalize
	};
	return &GstMfxSurfaceProxyClass;
}

static void
gst_mfx_surface_proxy_init_properties(GstMfxSurfaceProxy * proxy)
{
	proxy->format = gst_video_format_from_mfx_fourcc(proxy->surface->Info.FourCC);
	proxy->width = proxy->surface->Info.Width;
	proxy->height = proxy->surface->Info.Height;

	proxy->crop_rect.x = proxy->surface->Info.CropX;
	proxy->crop_rect.y = proxy->surface->Info.CropY;
	proxy->crop_rect.width = proxy->surface->Info.CropW;
	proxy->crop_rect.height = proxy->surface->Info.CropH;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new(GstMfxContextAllocator * ctx)
{
	GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(ctx != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->pool = NULL;
	proxy->ctx = ctx;
	if (!mfx_surface_create(proxy))
		goto error;
	gst_mfx_surface_proxy_init_properties(proxy);
	return proxy;

error:
	gst_mfx_surface_proxy_unref(proxy);
	return NULL;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_from_pool(GstMfxSurfacePool * pool)
{
	GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(pool != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->pool = gst_mfx_object_pool_ref(pool);
	proxy = gst_mfx_object_pool_get_object(proxy->pool);
	if (!proxy)
		goto error;
	gst_mfx_surface_proxy_init_properties(proxy);
	return proxy;

error:
	gst_mfx_surface_proxy_unref(proxy);
	return NULL;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_copy(GstMfxSurfaceProxy * proxy)
{
	GstMfxSurfaceProxy *copy;

	g_return_val_if_fail(proxy != NULL, NULL);

	copy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new(gst_mfx_surface_proxy_class());
	if (!copy)
		return NULL;

	copy->pool = proxy->pool ? gst_mfx_object_pool_ref(proxy->pool) : NULL;
	copy->ctx = proxy->ctx;
	copy->surface = proxy->surface;
	copy->format = proxy->format;
	copy->width = proxy->width;
	copy->height = proxy->height;
	copy->crop_rect = proxy->crop_rect;

	return copy;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_ref(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return
		GST_MFX_SURFACE_PROXY(gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT
		(proxy)));
}

void
gst_mfx_surface_proxy_unref(GstMfxSurfaceProxy * proxy)
{
	g_return_if_fail(proxy != NULL);

	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(proxy));
}

void
gst_mfx_surface_proxy_replace(GstMfxSurfaceProxy ** old_proxy_ptr,
	GstMfxSurfaceProxy * new_proxy)
{
	g_return_if_fail(old_proxy_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_proxy_ptr,
		GST_MFX_MINI_OBJECT(new_proxy));
}

mfxFrameSurface1 *
gst_mfx_surface_proxy_get_frame_surface(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return proxy->surface;
}

GstMfxID
gst_mfx_surface_proxy_get_id(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, VA_INVALID_ID);
	g_return_val_if_fail(proxy->surface != NULL, VA_INVALID_ID);

	return *(GstMfxID *)proxy->surface->Data.MemId;
}

const GstMfxRectangle *
gst_mfx_surface_proxy_get_crop_rect(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return &proxy->crop_rect;
}

GstVideoFormat
gst_mfx_surface_proxy_get_format(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return proxy->format;
}

guint
gst_mfx_surface_proxy_get_width(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return proxy->width;
}

guint
gst_mfx_surface_proxy_get_height(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, 0);

	return proxy->height;
}

void
gst_mfx_surface_proxy_get_size(GstMfxSurfaceProxy * proxy,
	guint * width_ptr, guint * height_ptr)
{
	g_return_if_fail(proxy != NULL);

	if (width_ptr)
		*width_ptr = proxy->width;

	if (height_ptr)
		*height_ptr = proxy->height;
}

GstMfxContextAllocator *
gst_mfx_surface_proxy_get_allocator_context(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return proxy->ctx;
}

GstVaapiImage *
gst_mfx_surface_proxy_derive_image(GstMfxSurfaceProxy * proxy)
{
	GstMfxDisplay *display;
	VAImage va_image;
	VAStatus status;

	g_return_val_if_fail(proxy != NULL, NULL);

	display = proxy->ctx->display;
	va_image.image_id = VA_INVALID_ID;
	va_image.buf = VA_INVALID_ID;

	GST_MFX_DISPLAY_LOCK(display);
	status = vaDeriveImage(GST_MFX_DISPLAY_VADISPLAY(display),
		GST_MFX_SURFACE_PROXY_MEMID(proxy), &va_image);
	GST_MFX_DISPLAY_UNLOCK(display);
	if (!vaapi_check_status(status, "vaDeriveImage()")) {
		return NULL;
	}
	if (va_image.image_id == VA_INVALID_ID || va_image.buf == VA_INVALID_ID)
		return NULL;

	return gst_vaapi_image_new_with_image(display, &va_image);
}

