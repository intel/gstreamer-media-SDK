#include "gstmfxcontext.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_CONTEXT_CAST(obj) \
	((GstMfxContext *)(obj))

#define GST_MFX_CONTEXT_GET_PRIVATE(obj) \
	(&GST_MFX_CONTEXT_CAST(obj)->priv)

typedef struct _GstMfxContextPrivate GstMfxContextPrivate;

struct _GstMfxContextPrivate {
	GstMfxDisplay   *display;
	VASurfaceID     *surfaces;
    mfxMemId        *surface_ids;
	mfxFrameInfo     frame_info;
	int              num_surfaces;
	GAsyncQueue     *surface_queue;
	mfxSession       session;
};

/**
* GstMfxContext:
*
* An MFX context wrapper.
*/
struct _GstMfxContext
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

    GstMfxContextPrivate priv;
};

static mfxStatus
frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req,
    mfxFrameAllocResponse *resp)
{
    GstMfxContextPrivate *ctx = pthis;
    VAStatus sts;
    guint i;

	if (ctx->surfaces) {
        GST_ERROR("Multiple allocation requests.\n");
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (!(req->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)) {
        GST_ERROR("Unsupported surface type: %d\n", req->Type);
        return MFX_ERR_UNSUPPORTED;
    }

    if (req->Info.FourCC != MFX_FOURCC_NV12 ||
        req->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420) {
        GST_ERROR("Unsupported surface properties.\n");
        return MFX_ERR_UNSUPPORTED;
    }

	ctx->num_surfaces = req->NumFrameSuggested;
	ctx->frame_info = req->Info;

	ctx->surfaces = g_slice_alloc(ctx->num_surfaces * sizeof(*ctx->surfaces));
	ctx->surface_ids = g_slice_alloc(ctx->num_surfaces * sizeof(*ctx->surface_ids));
	ctx->surface_queue = g_async_queue_new();

	if (!ctx->surfaces || !ctx->surface_ids || !ctx->surface_queue)
        goto fail;

	GST_MFX_DISPLAY_LOCK(ctx->display);
	sts = vaCreateSurfaces(GST_MFX_DISPLAY_VADISPLAY(ctx->display),
				VA_RT_FORMAT_YUV420,
				req->Info.Width, req->Info.Height,
				ctx->surfaces, ctx->num_surfaces,
				NULL, 0);
	GST_MFX_DISPLAY_UNLOCK(ctx->display);
	if (!vaapi_check_status(sts, "vaCreateSurfaces()")) {
		GST_ERROR("Error allocating VA surfaces\n");
		goto fail;
	}

	for (i = 0; i < ctx->num_surfaces; i++) {
		ctx->surface_ids[i] = &ctx->surfaces[i];
		g_async_queue_push(ctx->surface_queue, ctx->surface_ids[i]);
    }

    resp->mids           = ctx->surface_ids;
	resp->NumFrameActual = ctx->num_surfaces;

    return MFX_ERR_NONE;
fail:
	g_slice_free1(ctx->num_surfaces * sizeof(*ctx->surfaces), ctx->surfaces);
	g_slice_free1(ctx->num_surfaces * sizeof(*ctx->surface_ids), ctx->surface_ids);
	g_async_queue_unref(ctx->surface_queue);

    return MFX_ERR_MEMORY_ALLOC;
}

static mfxStatus
frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp)
{
	GstMfxContextPrivate *ctx = pthis;
	gpointer surface;

	GST_MFX_DISPLAY_LOCK(ctx->display);
	if (ctx->surfaces)
		vaDestroySurfaces(GST_MFX_DISPLAY_VADISPLAY(ctx->display), ctx->surfaces, ctx->num_surfaces);
	GST_MFX_DISPLAY_UNLOCK(ctx->display);

	g_slice_free1(ctx->num_surfaces * sizeof(*ctx->surfaces), ctx->surfaces);
	g_slice_free1(ctx->num_surfaces * sizeof(*ctx->surface_ids), ctx->surface_ids);
	g_async_queue_unref(ctx->surface_queue);

	ctx->num_surfaces = 0;

    return MFX_ERR_NONE;
}

static mfxStatus
frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    return MFX_ERR_UNSUPPORTED;
}

static mfxStatus
frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    return MFX_ERR_UNSUPPORTED;
}

static mfxStatus
frame_get_hdl(mfxHDL pthis, mfxMemId mid, mfxHDL *hdl)
{
    *hdl = mid;
    return MFX_ERR_NONE;
}

static void
gst_mfx_context_finalize(GstMfxContext * context)
{
	MFXClose(context->priv.session);
	gst_mfx_display_unref(context->priv.display);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_context_class(void)
{
	static const GstMfxMiniObjectClass GstMfxContextClass = {
		sizeof(GstMfxContext),
		(GDestroyNotify)gst_mfx_context_finalize
	};
	return &GstMfxContextClass;
}

static gboolean
gst_mfx_context_init_session(GstMfxContext * context)
{
    GstMfxContextPrivate *const priv = GST_MFX_CONTEXT_GET_PRIVATE(context);
	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { { 1, 1 } };

	const char *desc;
	int ret;

	ret = MFXInit(impl, &ver, &priv->session);
	if (ret < 0) {
		GST_ERROR("Error initializing internal MFX session");
		return FALSE;
	}

	MFXQueryIMPL(priv->session, &impl);

	switch (MFX_IMPL_BASETYPE(impl)) {
	case MFX_IMPL_SOFTWARE:
		desc = "software";
		break;
	case MFX_IMPL_HARDWARE:
	case MFX_IMPL_HARDWARE2:
	case MFX_IMPL_HARDWARE3:
	case MFX_IMPL_HARDWARE4:
		desc = "hardware accelerated";
		break;
	default:
		desc = "unknown";
	}

	GST_INFO("Initialized internal MFX session using %s implementation", desc);

	return TRUE;
}

static gboolean
context_create(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, FALSE);

	GstMfxContextPrivate *const priv = GST_MFX_CONTEXT_GET_PRIVATE(context);

    memset(priv, 0, sizeof (GstMfxContextPrivate));

    priv->display = gst_mfx_display_drm_new(NULL);
    if (!priv->display)
        return FALSE;

	if (!gst_mfx_context_init_session(context))
		return FALSE;

    mfxFrameAllocator frame_allocator = {
        .pthis = priv,
        .Alloc = frame_alloc,
        .Lock = frame_lock,
        .Unlock = frame_unlock,
        .GetHDL = frame_get_hdl,
        .Free = frame_free,
    };

    MFXVideoCORE_SetFrameAllocator(priv->session, &frame_allocator);

	return TRUE;
}

GstMfxContext *
gst_mfx_context_new(void)
{
	GstMfxContext *context;

	context = gst_mfx_mini_object_new(gst_mfx_context_class());
	if (!context)
		return NULL;

	if (!context_create(context))
		goto error;

	return context;

error:
	gst_mfx_context_unref(context);
	return NULL;
}

GstMfxContext *
gst_mfx_context_ref(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, NULL);

	return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(context));
}

void
gst_mfx_context_unref(GstMfxContext * context)
{
	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(context));
}

void
gst_mfx_context_replace(GstMfxContext ** old_context_ptr,
	GstMfxContext * new_context)
{
	g_return_if_fail(old_context_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_context_ptr,
		GST_MFX_MINI_OBJECT(new_context));
}

mfxSession
gst_mfx_context_get_session(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, 0);

	return GST_MFX_CONTEXT_GET_PRIVATE(context)->session;
}

GstMfxDisplay *
gst_mfx_context_get_display(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, 0);

	return GST_MFX_CONTEXT_GET_PRIVATE(context)->display;
}

GAsyncQueue *
gst_mfx_context_get_surfaces(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, 0);

	return GST_MFX_CONTEXT_GET_PRIVATE(context)->surface_queue;
}

mfxFrameInfo *
gst_mfx_context_get_frame_info(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, 0);

	return &GST_MFX_CONTEXT_GET_PRIVATE(context)->frame_info;
}
