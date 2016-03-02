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
typedef struct _GstMfxContextCurrent GstMfxContextCurrent;

typedef enum {
    GST_MFX_CONTEXT_DECODER = 0,
    GST_MFX_CONTEXT_VPP,
    GST_MFX_CONTEXT_ENCODER,
} GstMfxContextType;

GstMfxContext *
gst_mfx_context_new(void);

GstMfxContext *
gst_mfx_context_ref(GstMfxContext * context);

void
gst_mfx_context_unref(GstMfxContext * context);

void
gst_mfx_context_replace(GstMfxContext ** old_context_ptr,
	GstMfxContext * new_context);

GstMfxContextCurrent *
gst_mfx_context_get_current(GstMfxContext * context);

GstMfxDisplay *
gst_mfx_context_get_display(GstMfxContext * context);

GAsyncQueue *
gst_mfx_context_get_surfaces(GstMfxContext * context);

mfxSession
gst_mfx_context_get_session(GstMfxContext * context);

mfxFrameInfo *
gst_mfx_context_get_frame_info(GstMfxContext * context);

gboolean
gst_mfx_context_initialize(GstMfxContext * context,
    GstMfxContextType type, mfxSession * session);

mfxStatus
gst_mfx_context_frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req,
    mfxFrameAllocResponse *resp);

mfxStatus
gst_mfx_context_frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp);

mfxStatus
gst_mfx_context_frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);

mfxStatus
gst_mfx_context_frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);


G_END_DECLS

#endif /* GST_MFX_CONTEXT_H */
