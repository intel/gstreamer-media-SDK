#ifndef GST_MFX_CONTEXT_H
#define GST_MFX_CONTEXT_H

#include "gstmfxminiobject.h"
#include "gstmfxdisplay.h"

#include <mfxvideo.h>
#include <va/va.h>

G_BEGIN_DECLS

#define GST_MFX_CONTEXT(obj) \
	((GstMfxContext *) (obj))

#define GST_MFX_CONTEXT_DISPLAY(context) \
    gst_mfx_context_get_display(context)

#define GST_MFX_CONTEXT_SURFACES(context) \
    gst_mfx_context_get_surfaces(context)

#define GST_MFX_CONTEXT_SESSION(context) \
    gst_mfx_context_get_session(context)

typedef struct _GstMfxContext GstMfxContext;

GstMfxContext *
gst_mfx_context_new(void);

GstMfxContext *
gst_mfx_context_ref(GstMfxContext * context);

void
gst_mfx_context_unref(GstMfxContext * context);

void
gst_mfx_context_replace(GstMfxContext ** old_context_ptr,
	GstMfxContext * new_context);

GstMfxDisplay *
gst_mfx_context_get_display(GstMfxContext * context);

GAsyncQueue *
gst_mfx_context_get_surfaces(GstMfxContext * context);

mfxSession
gst_mfx_context_get_session(GstMfxContext * context);

mfxFrameInfo *
gst_mfx_context_get_frame_info(GstMfxContext * context);

G_END_DECLS

#endif /* GST_MFX_CONTEXT_H */
