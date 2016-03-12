#include "gstmfxtaskaggregator.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/**
* GstMfxTaskAggregator:
*
* An MFX aggregator wrapper.
*/
struct _GstMfxTaskAggregator
{
	/*< private >*/
	GstMfxMiniObject		 parent_instance;

    GstMfxDisplay			*display;
	GList					*cache;
	GstMfxTask			    *current;
};

static void
gst_mfx_task_aggregator_finalize(GstMfxTaskAggregator * aggregator)
{
	g_list_free_full(aggregator->cache, gst_mfx_mini_object_unref);
	gst_mfx_display_unref(aggregator->display);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_task_aggregator_class(void)
{
	static const GstMfxMiniObjectClass GstMfxTaskAggregatorClass = {
		sizeof(GstMfxTaskAggregator),
		(GDestroyNotify)gst_mfx_task_aggregator_finalize
	};
	return &GstMfxTaskAggregatorClass;
}

static gboolean
aggregator_create(GstMfxTaskAggregator * aggregator)
{
	g_return_val_if_fail(aggregator != NULL, FALSE);

	aggregator->cache = NULL;
    aggregator->display = gst_mfx_display_drm_new(NULL);
    if (!aggregator->display)
        return FALSE;

    return TRUE;
}

GstMfxTaskAggregator *
gst_mfx_task_aggregator_new(void)
{
	GstMfxTaskAggregator *aggregator;

	aggregator = gst_mfx_mini_object_new(gst_mfx_task_aggregator_class());
	if (!aggregator)
		return NULL;

	if (!aggregator_create(aggregator))
		goto error;

	return aggregator;
error:
	gst_mfx_task_aggregator_unref(aggregator);
	return NULL;
}

GstMfxTaskAggregator *
gst_mfx_task_aggregator_ref(GstMfxTaskAggregator * aggregator)
{
	g_return_val_if_fail(aggregator != NULL, NULL);

	return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(aggregator));
}

void
gst_mfx_task_aggregator_unref(GstMfxTaskAggregator * aggregator)
{
	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(aggregator));
}

void
gst_mfx_task_aggregator_replace(GstMfxTaskAggregator ** old_aggregator_ptr,
	GstMfxTaskAggregator * new_aggregator)
{
	g_return_if_fail(old_aggregator_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_aggregator_ptr,
		GST_MFX_MINI_OBJECT(new_aggregator));
}

GstMfxDisplay *
gst_mfx_task_aggregator_get_display(GstMfxTaskAggregator * aggregator)
{
	g_return_val_if_fail(aggregator != NULL, 0);

	return aggregator->display;
}

static gint
find_task(gconstpointer cur, gconstpointer task)
{
	GstMfxTask *_cur = (GstMfxTask *) cur;
	GstMfxTask *_task = (GstMfxTask *) task;

	return ( (gst_mfx_task_get_session(_cur) !=
			  gst_mfx_task_get_session(_task)) ||
		    !(gst_mfx_task_has_type(_cur,
			  gst_mfx_task_get_task_type(_task))) );
}

GstMfxTask *
gst_mfx_task_aggregator_find_task(GstMfxTaskAggregator * aggregator,
	mfxSession * session, guint type_flags)
{
	GstMfxTask *task;

    g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(aggregator != NULL, NULL);

	task = gst_mfx_task_new_with_session(aggregator, session, type_flags);
	if (!task)
		return NULL;

	GList *l = g_list_find_custom(aggregator->cache, task,
		find_task);

	gst_mfx_task_unref(task);

	return (l != NULL ? gst_mfx_task_ref((GstMfxTask *)(l->data)) : NULL);
}

GstMfxTask *
gst_mfx_task_aggregator_get_current_task(GstMfxTaskAggregator * aggregator)
{
	return aggregator->current;
}

gboolean
gst_mfx_task_aggregator_set_current_task(GstMfxTaskAggregator * aggregator, GstMfxTask * task)
{
	mfxSession session;

	g_return_val_if_fail(aggregator != NULL, FALSE);
	g_return_val_if_fail(task != NULL, FALSE);

	session = gst_mfx_task_get_session(task);

	if (!gst_mfx_task_aggregator_find_task(aggregator,
		&session, gst_mfx_task_get_task_type(task)))
		aggregator->cache = g_list_prepend(aggregator->cache, task);

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
		GST_MFX_DISPLAY_VADISPLAY(aggregator->display));

	aggregator->current = task;

	return TRUE;
}
