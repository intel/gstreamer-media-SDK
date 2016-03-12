#ifndef GST_MFX_CONTEXT_H
#define GST_MFX_CONTEXT_H

#include "gstmfxminiobject.h"
#include "gstmfxdisplay.h"
#include "gstmfxtask.h"

#include <mfxvideo.h>
#include <va/va.h>

G_BEGIN_DECLS

#define GST_MFX_CONTEXT(obj) \
	((GstMfxContext *) (obj))

#define GST_MFX_CONTEXT_DISPLAY(context) \
    gst_mfx_context_get_display(context)

GstMfxContext *
gst_mfx_context_new(void);

GstMfxTask *
gst_mfx_context_get_current(GstMfxContext * context);

gboolean
gst_mfx_context_set_current(GstMfxContext * context,
	GstMfxTask * task);

GstMfxContext *
gst_mfx_context_ref(GstMfxContext * context);

void
gst_mfx_context_unref(GstMfxContext * context);

void
gst_mfx_context_replace(GstMfxContext ** old_context_ptr,
	GstMfxContext * new_context);

GstMfxDisplay *
gst_mfx_context_get_display(GstMfxContext * context);

GstMfxTask *
gst_mfx_context_find_task(GstMfxContext * context,
	mfxSession * session, guint type_flags);

G_END_DECLS

#endif /* GST_MFX_CONTEXT_H */
