#ifndef GST_MFX_TASK_AGGREGATOR_H
#define GST_MFX_TASK_AGGREGATOR_H

#include "gstmfxminiobject.h"
#include "gstmfxdisplay.h"
#include "gstmfxtask.h"

#include <mfxvideo.h>
#include <va/va.h>

G_BEGIN_DECLS

#define GST_MFX_TASK_AGGREGATOR(obj) \
	((GstMfxTaskAggregator *) (obj))

#define GST_MFX_TASK_AGGREGATOR_DISPLAY(aggregator) \
    gst_mfx_task_aggregator_get_display(aggregator)

GstMfxTaskAggregator *
gst_mfx_task_aggregator_new(void);

GstMfxTask *
gst_mfx_task_aggregator_get_current_task(GstMfxTaskAggregator * aggregator);

gboolean
gst_mfx_task_aggregator_set_current_task(GstMfxTaskAggregator * aggregator,
	GstMfxTask * task);

GstMfxTaskAggregator *
gst_mfx_task_aggregator_ref(GstMfxTaskAggregator * aggregator);

void
gst_mfx_task_aggregator_unref(GstMfxTaskAggregator * aggregator);

void
gst_mfx_task_aggregator_replace(GstMfxTaskAggregator ** old_aggregator_ptr,
	GstMfxTaskAggregator * new_aggregator);

GstMfxDisplay *
gst_mfx_task_aggregator_get_display(GstMfxTaskAggregator * aggregator);

GstMfxTask *
gst_mfx_task_aggregator_find_task(GstMfxTaskAggregator * aggregator,
	mfxSession * session, guint type_flags);

G_END_DECLS

#endif /* GST_MFX_TASK_AGGREGATOR_H */
