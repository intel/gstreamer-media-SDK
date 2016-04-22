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
	GstMfxDisplay      *display;

	mfxFrameSurface1    surface;
	GstVideoFormat      format;
	GstMfxRectangle     crop_rect;
    guint               width;
	guint               height;
	guint               data_size;
	guchar             *data;
	guchar             *planes[3];
	guint16             pitches[3];
};

static gboolean
gst_mfx_surface_proxy_map(GstMfxSurfaceProxy * proxy)
{
	mfxFrameData *ptr = &proxy->surface.Data;
	mfxFrameInfo *info = &proxy->surface.Info;
	guint frame_size;
	gboolean success = TRUE;

    frame_size = GST_ROUND_UP_16(info->Width) *
            GST_ROUND_UP_16(info->Height);

    switch (info->FourCC) {
    case MFX_FOURCC_NV12:
        proxy->data_size = frame_size * 3 / 2;
        proxy->data = (guchar *)g_slice_alloc(proxy->data_size);
        ptr->Pitch = proxy->pitches[0] = proxy->pitches[1] =
            GST_ROUND_UP_16(info->Width);

		ptr->Y = proxy->planes[0] = proxy->data + 1;
        ptr->U = proxy->planes[1] = ptr->Y + frame_size + 1;
        ptr->V = ptr->U + 1;

        break;
    case MFX_FOURCC_YV12:
        proxy->data_size = frame_size * 3 / 2;
        proxy->data = (guchar *)g_slice_alloc(proxy->data_size);
        ptr->Pitch = proxy->pitches[0] = GST_ROUND_UP_16(info->Width);
        proxy->pitches[1] = proxy->pitches[2] = ptr->Pitch / 2;

        ptr->Y = proxy->planes[0] = proxy->data + 1;
        ptr->V = proxy->planes[1] = ptr->Y + frame_size + 1;
        ptr->U = proxy->planes[2] = ptr->V + (frame_size / 4) + 1;

        break;
    case MFX_FOURCC_YUY2:
        proxy->data_size = frame_size * 2;
        proxy->data = (guchar *)g_slice_alloc(proxy->data_size);
        ptr->Pitch = proxy->pitches[0] = GST_ROUND_UP_16(info->Width) * 2;

        ptr->Y = proxy->planes[0] = proxy->data + 4;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;

        break;
    case MFX_FOURCC_UYVY:
        proxy->data_size = frame_size * 2;
        proxy->data = (guchar *)g_slice_alloc(proxy->data_size);
        ptr->Pitch = proxy->pitches[0] = GST_ROUND_UP_16(info->Width) * 2;

        ptr->U = proxy->planes[0] = proxy->data + 4;
        ptr->Y = ptr->U + 1;
        ptr->V = ptr->U + 2;

        break;
    case MFX_FOURCC_RGB4:
        proxy->data_size = frame_size * 4;
        proxy->data = (guchar *)g_slice_alloc(proxy->data_size);
        ptr->Pitch = proxy->pitches[0] = GST_ROUND_UP_16(info->Width) * 4;

        ptr->B = proxy->planes[0] = proxy->data + 4;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;
    default:
        success = FALSE;
        break;
    }

	return success;
}

static void
gst_mfx_surface_proxy_unmap(GstMfxSurfaceProxy * proxy)
{
	mfxFrameData *ptr = &proxy->surface.Data;

	if (NULL != ptr) {
		ptr->Pitch = 0;
		if (proxy->data)
            g_slice_free1(proxy->data_size, proxy->data);
		ptr->Y = NULL;
		ptr->U = NULL;
		ptr->V = NULL;
		ptr->A = NULL;
	}
}

static gboolean
mfx_surface_proxy_create_from_task(GstMfxSurfaceProxy * proxy)
{
    proxy->surface.Info = gst_mfx_task_get_request(proxy->task)->Info;

    if (gst_mfx_task_has_mapped_surface(proxy->task)) {
        gst_mfx_surface_proxy_map(proxy);
    }
    else {
        mfxMemId mem_id;

        mem_id = g_queue_pop_head(
            gst_mfx_task_get_surfaces(proxy->task));
        if (!mem_id)
            return FALSE;

        proxy->surface.Data.MemId = mem_id;
    }

	return TRUE;
}

static void
gst_mfx_surface_proxy_derive_mfx_frame_info(GstMfxSurfaceProxy * proxy, GstVideoInfo * info)
{
    mfxFrameInfo *frame_info = &proxy->surface.Info;

	frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	frame_info->FourCC = gst_video_format_to_mfx_fourcc(
        GST_VIDEO_INFO_FORMAT(info));
	frame_info->PicStruct = GST_VIDEO_INFO_IS_INTERLACED(info) ?
        (GST_VIDEO_INFO_FLAG_IS_SET(info, GST_VIDEO_FRAME_FLAG_TFF) ?
            MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF)
        : MFX_PICSTRUCT_PROGRESSIVE;

	frame_info->CropX = 0;
	frame_info->CropY = 0;
	frame_info->CropW = info->width;
	frame_info->CropH = info->height;
	frame_info->FrameRateExtN = info->fps_n;
	frame_info->FrameRateExtD = info->fps_d;
	frame_info->AspectRatioW = info->par_n;
	frame_info->AspectRatioH = info->par_d;
	frame_info->BitDepthChroma = 8;
	frame_info->BitDepthLuma = 8;

	frame_info->Width = GST_ROUND_UP_16(info->width);
	frame_info->Height =
		(MFX_PICSTRUCT_PROGRESSIVE == frame_info->PicStruct) ?
		GST_ROUND_UP_16(info->height) :
		GST_ROUND_UP_32(info->height);
}

static gboolean
mfx_surface_proxy_create(GstMfxSurfaceProxy * proxy,
    GstVideoInfo * info)
{
	gst_mfx_surface_proxy_derive_mfx_frame_info(proxy, info);

    if (!proxy->display)
        gst_mfx_surface_proxy_map(proxy);
    else {

    }

	return TRUE;
}

static void
gst_mfx_surface_proxy_finalize(GstMfxSurfaceProxy * proxy)
{
    if (proxy->pool) {
        gst_mfx_surface_pool_put_surface(proxy->pool, proxy);
        gst_mfx_surface_pool_replace(&proxy->pool, NULL);
    }

    if (proxy->data_size)
        gst_mfx_surface_proxy_unmap(proxy);

    if (proxy->task) {
        if (!gst_mfx_task_has_mapped_surface(proxy->task))
            g_queue_push_tail(gst_mfx_task_get_surfaces(proxy->task),
                GST_MFX_SURFACE_PROXY_MEMID(proxy));

        gst_mfx_task_replace(proxy->task, NULL);
    }
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
    mfxFrameInfo *info = &proxy->surface.Info;

	proxy->format = gst_video_format_from_mfx_fourcc(info->FourCC);
	proxy->width = info->Width;
	proxy->height = info->Height;

	proxy->crop_rect.x = info->CropX;
	proxy->crop_rect.y = info->CropY;
	proxy->crop_rect.width = info->CropW;
	proxy->crop_rect.height = info->CropH;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new (GstMfxDisplay * display, GstVideoInfo * info)
{
    GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(info != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new0(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

    if (display)
        proxy->display = gst_mfx_display_ref(display);

    if (!mfx_surface_proxy_create(proxy, info))
		goto error;
    gst_mfx_surface_proxy_init_properties(proxy);

    return proxy;
error:
	gst_mfx_surface_proxy_unref(proxy);
	return NULL;
}

GstMfxSurfaceProxy *
gst_mfx_surface_proxy_new_from_task (GstMfxTask * task)
{
	GstMfxSurfaceProxy *proxy;

	g_return_val_if_fail(task != NULL, NULL);

	proxy = (GstMfxSurfaceProxy *)
		gst_mfx_mini_object_new0(gst_mfx_surface_proxy_class());
	if (!proxy)
		return NULL;

	proxy->pool = NULL;
	proxy->task = gst_mfx_task_ref(task);
	if (!mfx_surface_proxy_create_from_task(proxy))
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

	return proxy->surface.Data.MemId ? *(GstMfxID *)proxy->surface.Data.MemId :
	    GST_MFX_ID_INVALID;
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
gst_mfx_surface_proxy_get_data(GstMfxSurfaceProxy * proxy)
{
    g_return_val_if_fail(proxy != NULL, NULL);

    return proxy->data;
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

