#include "gstmfxcontext.h"

static mfxStatus
frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req,
    mfxFrameAllocResponse *resp)
{
    GstMfxContextAllocatorVaapi *ctx = pthis;
    mfxU16 surfaces_num = req->NumFrameSuggested;
    int err, i;

    if (ctx->surfaces) {
        g_printerr("Multiple allocation requests.\n");
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (!(req->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)) {
        g_printerr("Unsupported surface type: %d\n", req->Type);
        return MFX_ERR_UNSUPPORTED;
    }

    if (req->Info.FourCC != MFX_FOURCC_NV12 || req->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420) {
        g_printerr("Unsupported surface properties.\n");
        return MFX_ERR_UNSUPPORTED;
    }

    ctx->surfaces     = g_malloc_n(surfaces_num, sizeof(*ctx->surfaces));
    ctx->surface_ids  = g_malloc_n(surfaces_num, sizeof(*ctx->surface_ids));
    ctx->surface_queue = g_async_queue_new();

    if (!ctx->surfaces || !ctx->surface_ids)
        goto fail;

    err = vaCreateSurfaces(ctx->va_dpy, VA_RT_FORMAT_YUV420,
                           req->Info.Width, req->Info.Height,
                           ctx->surfaces, surfaces_num,
                           NULL, 0);

    if (err != VA_STATUS_SUCCESS) {
        g_printerr("Error allocating VA surfaces\n");
        goto fail;
    }

    ctx->nb_surfaces = surfaces_num;
    for (i = 0; i < ctx->nb_surfaces; i++) {
        ctx->surface_ids[i] = &ctx->surfaces[i];
        g_async_queue_push(ctx->surface_queue, ctx->surface_ids[i]);
    }

    resp->mids           = ctx->surface_ids;
    resp->NumFrameActual = ctx->nb_surfaces;

    ctx->frame_info = req->Info;

    return MFX_ERR_NONE;
fail:
    g_free(ctx->surfaces);
    g_free(ctx->surface_ids);

    return MFX_ERR_MEMORY_ALLOC;
}

static mfxStatus
frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp)
{
    GstMfxContextAllocatorVaapi *ctx = pthis;

    if (ctx->surfaces)
        vaDestroySurfaces(ctx->va_dpy, ctx->surfaces, ctx->nb_surfaces);

    g_free(ctx->surfaces);
    g_free(ctx->surface_ids);
    g_async_queue_unref(ctx->surface_queue);

    ctx->nb_surfaces = 0;

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
context_destroy(GstMfxContext * context)
{
	MFXClose(context->session);
}

static void
gst_mfx_context_finalize(GstMfxContext * context)
{
	context_destroy(context);
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
	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { { 1, 1 } };

	const char *desc;
	int ret;

	ret = MFXInit(impl, &ver, &(context->session));
	if (ret < 0) {
		//GST_ERROR_OBJECT(context, "Error initializing an internal MFX session");
		return FALSE;
	}

	MFXQueryIMPL(context->session, &impl);

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

	//GST_INFO_OBJECT(context->session, "Initialized an internal MFX session using %s implementation",
		//desc);

	return TRUE;
}

static gboolean
context_create(GstMfxContext * context, GstMfxContextAllocatorVaapi * allocator)
{
	g_return_val_if_fail(allocator != NULL, FALSE);

	if (!gst_mfx_context_init_session(context))
		goto error;

    mfxFrameAllocator frame_allocator = {
        .pthis = allocator,
        .Alloc = frame_alloc,
        .Lock = frame_lock,
        .Unlock = frame_unlock,
        .GetHDL = frame_get_hdl,
        .Free = frame_free,
    };

    MFXVideoCORE_SetFrameAllocator(context->session, &frame_allocator);

	return TRUE;
error:
	return FALSE;
}

GstMfxContext *
gst_mfx_context_new(GstMfxContextAllocatorVaapi * allocator)
{
	GstMfxContext *context;

	context = gst_mfx_mini_object_new(gst_mfx_context_class());
	if (!context)
		return NULL;

	if (!context_create(context, allocator))
		goto error;

	return context;

error:
	gst_mfx_mini_object_unref(context);
	return NULL;
}

mfxSession
gst_mfx_context_get_session(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, 0);

	return context->session;
}


