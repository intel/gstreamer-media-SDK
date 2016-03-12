#include "gstmfxcontext.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/**
* GstMfxContext:
*
* An MFX context wrapper.
*/
struct _GstMfxContext
{
	/*< private >*/
	GstMfxMiniObject		 parent_instance;

    GstMfxDisplay			*display;
	GList					*cache;
	GstMfxTask			    *current;
};

static void
gst_mfx_context_finalize(GstMfxContext * context)
{
	g_list_free_full(context->cache, gst_mfx_mini_object_unref);
	gst_mfx_display_unref(context->display);
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
context_create(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, FALSE);

	context->cache = NULL;
    context->display = gst_mfx_display_drm_new(NULL);
    if (!context->display)
        return FALSE;

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

GstMfxDisplay *
gst_mfx_context_get_display(GstMfxContext * context)
{
	g_return_val_if_fail(context != NULL, 0);

	return context->display;
}

static gint
find_task(gconstpointer cur, gconstpointer task)
{
	GstMfxTask *_cur = (GstMfxTask *) cur;
	GstMfxTask *_task = (GstMfxTask *) task;

	return ( (gst_mfx_task_get_session(_cur) !=
			  gst_mfx_task_get_session(_task)) ||
		    !(gst_mfx_task_has_type(_cur,
			  gst_mfx_task_get_type_flags(_task))) );
}

GstMfxTask *
gst_mfx_context_find_task(GstMfxContext * context,
	mfxSession * session, guint type_flags)
{
	GstMfxTask *task;

    g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(context != NULL, NULL);

	task = gst_mfx_task_new_with_session(context, session, type_flags);
	if (!task)
		return NULL;

	GList *l = g_list_find_custom(context->cache, task,
		find_task);

	gst_mfx_task_unref(task);

	return (l != NULL ? (GstMfxTask *)(l->data) : NULL);
}

GstMfxTask *
gst_mfx_context_get_current(GstMfxContext * context)
{
	return context->current;
}

gboolean
gst_mfx_context_set_current(GstMfxContext * context, GstMfxTask * task)
{
	mfxSession session;

	g_return_val_if_fail(context != NULL, FALSE);
	g_return_val_if_fail(task != NULL, FALSE);

	session = gst_mfx_task_get_session(task);

	if (!gst_mfx_context_find_task(context,
		&session, gst_mfx_task_get_type_flags(task)))
		context->cache = g_list_prepend(context->cache, task);

	mfxFrameAllocator frame_allocator = {
		.pthis = task,
		.Alloc = gst_mfx_task_frame_alloc,
		.Free = gst_mfx_task_frame_free,
		.Lock = gst_mfx_task_frame_lock,
		.Unlock = gst_mfx_task_frame_unlock,
		.GetHDL = gst_mfx_task_frame_get_hdl,
	};

	MFXVideoCORE_SetFrameAllocator(session, &frame_allocator);
	MFXVideoCORE_SetHandle(session, MFX_HANDLE_VA_DISPLAY,
		GST_MFX_DISPLAY_VADISPLAY(context->display));

	context->current = task;

	return TRUE;
}
