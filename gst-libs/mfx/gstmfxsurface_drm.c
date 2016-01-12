#include "sysdeps.h"
//#include "gstmfxsurface.h"
#include "gstmfxsurface_drm.h"
#include "gstmfxsurface_priv.h"
#include "gstvaapiimage_priv.h"
#include "gstvaapibufferproxy_priv.h"

static GstVaapiBufferProxy *
gst_mfx_surface_get_drm_buf_handle (GstMfxSurface * surface, guint type)
{
  GstVaapiBufferProxy *proxy;
  GstVaapiImage *image;

  image = gst_mfx_surface_derive_image (surface);
  if (!image)
    goto error_derive_image;

  proxy =
      gst_vaapi_buffer_proxy_new_from_object (GST_MFX_OBJECT (surface),
      image->image.buf, type, gst_mfx_object_unref, image);
  if (!proxy)
    goto error_alloc_export_buffer;
  return proxy;

  /* ERRORS */
error_derive_image:
  GST_ERROR ("failed to extract image handle from surface");
  return NULL;
error_alloc_export_buffer:
  GST_ERROR ("failed to allocate export buffer proxy");
  gst_mfx_object_unref (image);
  return NULL;
}

/**
 * gst_mfx_surface_get_dma_buf_handle:
 * @surface: a #GstMfxSurface
 *
 * If the underlying VA driver implementation supports it, this
 * function allows for returning a suitable dma_buf (DRM) buffer
 * handle as a #GstVaapiBufferProxy instance. The resulting buffer
 * handle is live until the last reference to the proxy gets
 * released. Besides, any further change to the parent VA @surface may
 * fail.
 *
 * Return value: the underlying buffer as a #GstVaapiBufferProxy
 * instance.
 */
GstVaapiBufferProxy *
gst_mfx_surface_get_dma_buf_handle (GstMfxSurface * surface)
{
  GstVaapiBufferProxy *proxy;
  g_return_val_if_fail (surface != NULL, NULL);

  proxy = gst_mfx_surface_get_drm_buf_handle (surface,
      GST_VAAPI_BUFFER_MEMORY_TYPE_DMA_BUF);
  return proxy;
}

/**
 * gst_mfx_surface_get_gem_buf_handle:
 * @surface: a #GstMfxSurface
 *
 * If the underlying VA driver implementation supports it, this
 * function allows for returning a suitable GEM buffer handle as a
 * #GstVaapiBufferProxy instance. The resulting buffer handle is live
 * until the last reference to the proxy gets released. Besides, any
 * further change to the parent VA @surface may fail.
 *
 * Return value: the underlying buffer as a #GstVaapiBufferProxy
 * instance.
 */
GstVaapiBufferProxy *
gst_mfx_surface_get_gem_buf_handle (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return gst_mfx_surface_get_drm_buf_handle (surface,
      GST_VAAPI_BUFFER_MEMORY_TYPE_GEM_BUF);
}

#if 0
static void
fill_video_info (GstVideoInfo * vip, GstVideoFormat format, guint width,
    guint height, gsize offset[GST_VIDEO_MAX_PLANES],
    gint stride[GST_VIDEO_MAX_PLANES])
{
  guint i;

  gst_video_info_init (vip);
  gst_video_info_set_format (vip, format, width, height);
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (vip); i++) {
    GST_VIDEO_INFO_PLANE_OFFSET (vip, i) = offset[i];
    GST_VIDEO_INFO_PLANE_STRIDE (vip, i) = stride[i];
  }
}

/**
 * gst_mfx_surface_new_with_dma_buf_handle:
 * @display: a #GstMfxDisplay
 * @fd: the DRM PRIME file descriptor
 * @size: the underlying DRM buffer size
 * @format: the desired surface format
 * @width: the desired surface width in pixels
 * @height: the desired surface height in pixels
 * @offset: the offsets to each plane
 * @stride: the pitches for each plane
 *
 * Creates a new #GstMfxSurface with an external DRM PRIME file
 * descriptor. The newly created VA surfaces owns the supplied buffer
 * handle.
 *
 * Return value: the newly allocated #GstMfxSurface object, or %NULL
 *   if creation from DRM PRIME fd failed, or is not supported
 */

GstMfxSurface *
gst_mfx_surface_new_with_dma_buf_handle (GstMfxDisplay * display,
    gint fd, guint size, GstVideoFormat format, guint width, guint height,
    gsize offset[GST_VIDEO_MAX_PLANES], gint stride[GST_VIDEO_MAX_PLANES])
{
  GstVaapiBufferProxy *proxy;
  GstMfxSurface *surface;
  GstVideoInfo vi;

  proxy = gst_vaapi_buffer_proxy_new ((gintptr) fd,
      GST_VAAPI_BUFFER_MEMORY_TYPE_DMA_BUF, size, NULL, NULL);
  if (!proxy)
    return NULL;

  fill_video_info (&vi, format, width, height, offset, stride);
  surface = gst_mfx_surface_new_from_buffer_proxy (display, proxy, &vi);
  gst_vaapi_buffer_proxy_unref (proxy);
  return surface;
}

/**
 * gst_mfx_surface_new_with_dma_buf_handle:
 * @display: a #GstMfxDisplay
 * @name: the DRM GEM buffer name
 * @size: the underlying DRM buffer size
 * @format: the desired surface format
 * @width: the desired surface width in pixels
 * @height: the desired surface height in pixels
 * @offset: the offsets to each plane
 * @stride: the pitches for each plane
 *
 * Creates a new #GstMfxSurface with an external DRM GEM buffer
 * name. The newly created VA surfaces owns the supplied buffer
 * handle.
 *
 * Return value: the newly allocated #GstMfxSurface object, or %NULL
 *   if creation from GEM FLINK failed, or is not supported
 */
GstMfxSurface *
gst_mfx_surface_new_with_gem_buf_handle (GstMfxDisplay * display,
    guint32 name, guint size, GstVideoFormat format, guint width, guint height,
    gsize offset[GST_VIDEO_MAX_PLANES], gint stride[GST_VIDEO_MAX_PLANES])
{
  GstVaapiBufferProxy *proxy;
  GstMfxSurface *surface;
  GstVideoInfo vi;

  proxy = gst_vaapi_buffer_proxy_new ((guintptr) name,
      GST_VAAPI_BUFFER_MEMORY_TYPE_GEM_BUF, size, NULL, NULL);
  if (!proxy)
    return NULL;

  fill_video_info (&vi, format, width, height, offset, stride);
  surface = gst_mfx_surface_new_from_buffer_proxy (display, proxy, &vi);
  gst_vaapi_buffer_proxy_unref (proxy);
  return surface;
}
#endif
