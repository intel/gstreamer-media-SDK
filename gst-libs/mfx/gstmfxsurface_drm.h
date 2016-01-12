#ifndef GST_MFX_SURFACE_DRM_H
#define GST_MFX_SURFACE_DRM_H

#include "gstmfxsurface.h"
#include "gstvaapibufferproxy.h"

G_BEGIN_DECLS

GstVaapiBufferProxy *
gst_mfx_surface_get_dma_buf_handle (GstMfxSurface * surface);

GstVaapiBufferProxy *
gst_mfx_surface_get_gem_buf_handle (GstMfxSurface * surface);

#if 0
GstMfxSurface *
gst_mfx_surface_new_with_dma_buf_handle (GstMfxDisplay * display,
    gint fd, guint size, GstVideoFormat format, guint width, guint height,
    gsize offset[GST_VIDEO_MAX_PLANES], gint stride[GST_VIDEO_MAX_PLANES]);

GstMfxSurface *
gst_mfx_surface_new_with_gem_buf_handle (GstMfxDisplay * display,
    guint32 name, guint size, GstVideoFormat format, guint width, guint height,
    gsize offset[GST_VIDEO_MAX_PLANES], gint stride[GST_VIDEO_MAX_PLANES]);
#endif

G_END_DECLS

#endif /* GST_MFX_SURFACE_DRM_H */
