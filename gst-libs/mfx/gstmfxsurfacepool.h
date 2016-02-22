#ifndef GST_MFX_SURFACE_POOL_H
#define GST_MFX_SURFACE_POOL_H

#include "gstmfxsurface.h"
#include "gstmfxobjectpool.h"
#include "video-utils.h"

G_BEGIN_DECLS

#define GST_MFX_SURFACE_POOL(obj) \
	((GstMfxSurfacePool *)(obj))

typedef struct _GstMfxSurfacePool GstMfxSurfacePool;

GstMfxObjectPool *
gst_mfx_surface_pool_new(GstMfxDisplay * display,
	GstMfxContextAllocatorVaapi * ctx);

G_END_DECLS

#endif /* GST_MFX_SURFACE_POOL_H */
