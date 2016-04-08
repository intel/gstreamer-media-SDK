#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxutils_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfaceProxy
{
	/*< private >*/
	GstMfxMiniObject    parent_instance;

	GstMfxTask         *task;
	GstMfxSurfacePool  *pool;

	mfxFrameSurface1    surface;
	GstVideoFormat      format;
	GstMfxRectangle     crop_rect;
    guint               width;
	guint               height;
	guint               data_size;
	guchar             *planes[4];
	guint16             pitches[4];
};

static gboolean
gst_mfx_surface_proxy_map(GstMfxSurfaceProxy * proxy)
{
	mfxFrameData *ptr = &proxy->surface.Data;
	mfxFrameInfo *info = &proxy->surface.Info;
	VaapiImage *image;
	gboolean success = TRUE;

	image = gst_mfx_surface_proxy_derive_image(proxy);
	if (!image)
        return FALSE;

    proxy->data_size = vaapi_image_get_data_size(image);

    switch (info->FourCC) {
    case MFX_FOURCC_NV12:
        ptr->Pitch = proxy->pitches[0] = (mfxU16)vaapi_image_get_pitch(image, 0);
        proxy->pitches[1] = (guint16)vaapi_image_get_pitch(image, 1);

        ptr->Y = proxy->planes[0] = g_slice_alloc(proxy->data_size);
        ptr->U = proxy->planes[1] = ptr->Y +
            vaapi_image_get_offset(image, 1) + 1;

        break;
    case MFX_FOURCC_YV12:
        ptr->Pitch = proxy->pitches[0] = (mfxU16)vaapi_image_get_pitch(image, 0);
        proxy->pitches[1] = (guint16)vaapi_image_get_pitch(image, 1);
        proxy->pitches[2] = (guint16)vaapi_image_get_pitch(image, 2);

        ptr->Y = proxy->planes[0] = g_slice_alloc(proxy->data_size);
        ptr->U = proxy->planes[1] = ptr->Y +
            vaapi_image_get_offset(image, 1);
        ptr->V = proxy->planes[2] = ptr->Y +
            vaapi_image_get_offset(image, 2);

        break;
    case MFX_FOURCC_YUY2:
        ptr->Pitch = proxy->pitches[0] = (mfxU16)vaapi_image_get_pitch(image, 0);

        ptr->Y = proxy->planes[0] = g_slice_alloc(proxy->data_size);
        ptr->U = proxy->planes[1] = ptr->Y + 1;
        ptr->V = proxy->planes[2] = ptr->Y + 3;

        break;
    case MFX_FOURCC_RGB4:
        ptr->Pitch = proxy->pitches[0] = (mfxU16)vaapi_image_get_pitch(image, 0);

        ptr->A = g_slice_alloc(proxy->data_size);
        ptr->R = proxy->planes[0] = ptr->A + 1;
        ptr->G = proxy->planes[1] = ptr->A + 2;
        ptr->B = proxy->planes[2] = ptr->A + 3;


        break;
    default:
        success = FALSE;
        break;
    }

    vaapi_image_unref(image);

	return success;
}

static void
gst_mfx_surface_proxy_unmap(GstMfxSurfaceProxy * proxy)
{
	mfxFrameData *ptr = &proxy->surface.Data;

	if (NULL != ptr) {
		ptr->Pitch = 0;
		if (ptr->Y)
            g_slice_free1(proxy->data_size, ptr->Y);
		ptr->Y = NULL;
		ptr->U = NULL;
		ptr->V = NULL;
		ptr->A = NULL;
	}
}


static gboolean
mfx_surface_create(GstMfxSurfaceProxy * proxy)
{
	mfxMemId mem_id;

	guint num = g_queue_get_length(gst_mfx_task_get_surfaces(proxy->task));

	mem_id = g_queue_pop_head(
		gst_mfx_task_get_surfaces(proxy->task));
	if (!mem_id)
		return FALSE;

	proxy->surface.Data.MemId = mem_id;
	proxy->surface.Info = *gst_mfx_task_get_frame_info(proxy->task);

	if (gst_mfx_task_has_system_memory(proxy->task))
		gst_mfx_surface_proxy_map(proxy);

	return TRUE;
}

static void
gst_mfx_surface_proxy_finalize(GstMfxSurfaceProxy * proxy)
{
    if (proxy->pool) {
        gst_mfx_surface_pool_put_surface(proxy->pool, proxy);
        gst_mfx_surface_pool_replace(&proxy->pool, NULL);
    }

    if (gst_mfx_task_has_system_memory(proxy->task))
        gst_mfx_surface_proxy_unmap(proxy);

    g_queue_push_tail(gst_mfx_task_get_surfaces(proxy->task),
        GST_MFX_SURFACE_PROXY_MEMID(proxy));

	gst_mfx_task_unref(proxy->task);
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
	proxy->format = gst_video_format_from_mfx_fourcc(proxy->surface.Info.FourCC);
	proxy->width = proxy->surface.Info.Width;
	proxy->height = proxy->surface.Info.Height;

	proxy->crop_rect.x = proxy->surface.Info.CropX;
	proxy->crop_rect.y = proxy->surface.Info.CropY;
	proxy->crop_rect.width = proxy->surface.Info.CropW;
	proxy->crop_rect.height = proxy->surface.Info.CropH;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new(GstMfxTask * task)
{
	GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(task != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new0(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->pool = NULL;
	proxy->task = gst_mfx_task_ref(task);
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
		gst_mfx_mini_object_new0(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->pool = gst_mfx_surface_pool_ref(pool);
	proxy = gst_mfx_surface_pool_get_surface(proxy->pool);
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
		gst_mfx_mini_object_new0(gst_mfx_surface_proxy_class());
	if (!copy)
		return NULL;

	copy->pool = proxy->pool ? gst_mfx_surface_pool_ref(proxy->pool) : NULL;
	copy->task = proxy->task ? gst_mfx_task_ref(proxy->task) : NULL;
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

	return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(proxy));
}

void
gst_mfx_surface_proxy_unref(GstMfxSurfaceProxy * proxy)
{
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

	return &proxy->surface;
}

GstMfxID
gst_mfx_surface_proxy_get_id(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, GST_MFX_ID_INVALID);

	return *(GstMfxID *)proxy->surface.Data.MemId;
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

guchar *
gst_mfx_surface_proxy_get_plane(GstMfxSurfaceProxy * proxy, guint plane)
{
    g_return_val_if_fail(proxy != NULL, NULL);
    g_return_val_if_fail(plane <= 4, NULL);

    return proxy->planes[plane];
}

guint16
gst_mfx_surface_proxy_get_pitch(GstMfxSurfaceProxy * proxy, guint plane)
{
    g_return_val_if_fail(proxy != NULL, NULL);
    g_return_val_if_fail(plane <= 4, NULL);

    return proxy->pitches[plane];
}

GstMfxTask *
gst_mfx_surface_proxy_get_task_context(GstMfxSurfaceProxy * proxy)
{
	g_return_val_if_fail(proxy != NULL, NULL);

	return proxy->task;
}

VaapiImage *
gst_mfx_surface_proxy_derive_image(GstMfxSurfaceProxy * proxy)
{
	GstMfxDisplay *display;
	VAImage va_image;
	VAStatus status;

	g_return_val_if_fail(proxy != NULL, NULL);

	display = GST_MFX_TASK_DISPLAY(proxy->task);
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

	return vaapi_image_new_with_image(display, &va_image);
}

