#ifndef GST_MFX_CONTEXT_H
#define GST_MFX_CONTEXT_H

#include "gstmfxminiobject.h"
#include "gstvaapialloc.h"
#include <mfxvideo.h>

G_BEGIN_DECLS

#define GST_MFX_CONTEXT(obj) \
	((GstMfxContext *) (obj))

typedef struct _GstMfxContextAllocator GstMfxContextAllocator;
typedef struct _GstMfxContext GstMfxContext;

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
gst_mfx_context_new(VaapiAllocatorContext *allocator);

mfxSession
gst_mfx_context_get_session(GstMfxContext * context);

G_END_DECLS

#endif /* GST_MFX_CONTEXT_H */
