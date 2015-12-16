#include "gstmfxcontext.h"

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
context_create(GstMfxContext * context, VaapiAllocatorContext * allocator)
{
	gboolean success = FALSE;

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

	success = TRUE;
error:
	return success;
}

GstMfxContext *
gst_mfx_context_new(VaapiAllocatorContext * allocator)
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


