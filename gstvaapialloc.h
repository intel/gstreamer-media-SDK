#ifndef __GSTVAAPIALLOC_H
#define __GSTVAAPIALLOC_H

#include <stdio.h>
#include <glib.h>

#include <mfxvideo.h>

#include <va/va.h>
#include <va/va_x11.h>

typedef struct VaapiAllocatorContext {
    VADisplay va_dpy;
    VASurfaceID *surfaces;
    mfxMemId    *surface_ids;
    GAsyncQueue *surface_queue;
    int nb_surfaces;
    mfxFrameInfo frame_info;
} VaapiAllocatorContext;

mfxStatus frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req,
    mfxFrameAllocResponse *resp);
mfxStatus frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp);
mfxStatus frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus frame_get_hdl(mfxHDL pthis, mfxMemId mid, mfxHDL *hdl);

#endif
