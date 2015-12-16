#include "gstvaapialloc.h"

mfxStatus frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req,
    mfxFrameAllocResponse *resp)
{
    VaapiAllocatorContext *decode = pthis;
    mfxU16 surfaces_num = req->NumFrameSuggested;
    int err, i;

    if (decode->surfaces) {
        g_printerr("Multiple allocation requests.\n");
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (!(req->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)) {
        g_printerr("Unsupported surface type: %d\n", req->Type);
        return MFX_ERR_UNSUPPORTED;
    }

    if (req->Info.FourCC != MFX_FOURCC_NV12 || req->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420) {
        g_printerr("Unsupported surface properties.\n");
        return MFX_ERR_UNSUPPORTED;
    }

    decode->surfaces     = g_malloc_n(surfaces_num, sizeof(*decode->surfaces));
    decode->surface_ids  = g_malloc_n(surfaces_num, sizeof(*decode->surface_ids));
    decode->surface_queue = g_async_queue_new();

    if (!decode->surfaces || !decode->surface_ids)
        goto fail;

    err = vaCreateSurfaces(decode->va_dpy, VA_RT_FORMAT_YUV420,
                           req->Info.Width, req->Info.Height,
                           decode->surfaces, surfaces_num,
                           NULL, 0);

    if (err != VA_STATUS_SUCCESS) {
        g_printerr("Error allocating VA surfaces\n");
        goto fail;
    }

    decode->nb_surfaces = surfaces_num;
    for (i = 0; i < decode->nb_surfaces; i++) {
        decode->surface_ids[i] = &decode->surfaces[i];
        g_async_queue_push(decode->surface_queue, decode->surface_ids[i]);
    }

    resp->mids           = decode->surface_ids;
    resp->NumFrameActual = decode->nb_surfaces;

    decode->frame_info = req->Info;

    return MFX_ERR_NONE;
fail:
    g_free(decode->surfaces);
    g_free(decode->surface_ids);

    return MFX_ERR_MEMORY_ALLOC;
}

mfxStatus frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp)
{
    VaapiAllocatorContext *decode = pthis;

    if (decode->surfaces)
        vaDestroySurfaces(decode->va_dpy, decode->surfaces, decode->nb_surfaces);

    g_free(decode->surfaces);
    g_free(decode->surface_ids);
    g_async_queue_unref(decode->surface_queue);

    decode->nb_surfaces = 0;

    return MFX_ERR_NONE;
}

mfxStatus frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus frame_get_hdl(mfxHDL pthis, mfxMemId mid, mfxHDL *hdl)
{
    *hdl = mid;
    return MFX_ERR_NONE;
}
