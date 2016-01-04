#ifndef GST_MFX_CONTEXT_H
#define GST_MFX_CONTEXT_H

#include "gstmfxminiobject.h"
#include "gstmfxcontext.h"
#include <mfxvideo.h>

#include <va/va.h>
#include <va/va_x11.h>

G_BEGIN_DECLS

#define GST_MFX_CONTEXT(obj) \
	((GstMfxContext *) (obj))

typedef struct _GstMfxContextAllocatorVaapi GstMfxContextAllocatorVaapi;
typedef struct _GstMfxContext GstMfxContext;

typedef struct _GstMfxContextAllocatorVaapi {
    VADisplay va_dpy;
    VASurfaceID *surfaces;
    mfxMemId    *surface_ids;
    GAsyncQueue *surface_queue;
    int nb_surfaces;
    mfxFrameInfo frame_info;
};


/**
* GstMfxContext:
*
* A VA context wrapper.
*/
struct _GstMfxContext
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	mfxSession session;
};

GstMfxContext *
gst_mfx_context_new(GstMfxContextAllocatorVaapi *allocator);

mfxSession
gst_mfx_context_get_session(GstMfxContext * context);

G_END_DECLS

#endif /* GST_MFX_CONTEXT_H */
