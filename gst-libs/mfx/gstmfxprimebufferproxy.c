/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include <gmodule.h>
#include <va/va_drmcommon.h>

#include "sysdeps.h"

#include "gstmfxutils_vaapi.h"
#include "gstmfxsurface_vaapi.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxprimebufferproxy.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxPrimeBufferProxy
{
  /*< private > */
  GstMfxMiniObject parent_instance;
  GstMfxSurface *surface;
  GstMfxDisplay *display;
  VaapiImage *image;
  VABufferInfo buf_info;
  guintptr fd;
  guint data_size;
};

typedef VAStatus (*vaExtGetSurfaceHandle) (VADisplay dpy,
    VASurfaceID * surface, int *prime_fd);

static vaExtGetSurfaceHandle g_va_get_surface_handle;

static gboolean
vpg_load_symbol (const gchar * vpg_extension)
{
  GModule *module;

  if (g_va_get_surface_handle)
    return TRUE;

  module = g_module_open ("iHD_drv_video.so", G_MODULE_BIND_LAZY |
      G_MODULE_BIND_LOCAL);
  if (!module)
    return FALSE;

  g_module_symbol (module, vpg_extension,
      (gpointer *) & g_va_get_surface_handle);
  if (!g_va_get_surface_handle) {
    g_module_close (module);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mfx_prime_buffer_proxy_acquire_handle (GstMfxPrimeBufferProxy * proxy)
{
  VASurfaceID surf;
  VAStatus va_status;
  VAImage va_img;

  if (!proxy->surface)
    return FALSE;

  surf = GST_MFX_SURFACE_ID (proxy->surface);
  proxy->display = gst_mfx_surface_vaapi_get_display (proxy->surface);
  proxy->image = gst_mfx_surface_vaapi_derive_image (proxy->surface);
  if (!proxy->image) {
    GST_ERROR("Could not derive image.");
    return FALSE;
  }
  vaapi_image_get_image (proxy->image, &va_img);

  if (vpg_load_symbol ("vpgExtGetSurfaceHandle")) {
    GST_MFX_DISPLAY_LOCK (proxy->display);
    va_status = g_va_get_surface_handle (GST_MFX_DISPLAY_VADISPLAY (proxy->display),
        &surf, (int *) &proxy->fd);
    GST_MFX_DISPLAY_UNLOCK (proxy->display);
    if (!vaapi_check_status (va_status, "vpgExtGetSurfaceHandle ()"))
      return FALSE;
  } else {
    proxy->buf_info.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    GST_MFX_DISPLAY_LOCK (proxy->display);
    va_status = vaAcquireBufferHandle (GST_MFX_DISPLAY_VADISPLAY (proxy->display),
        va_img.buf, &proxy->buf_info);
    GST_MFX_DISPLAY_UNLOCK (proxy->display);
    if (!vaapi_check_status (va_status, "vaAcquireBufferHandle ()"))
      return FALSE;
    proxy->fd = proxy->buf_info.handle;
  }

  proxy->data_size = va_img.data_size;

  return TRUE;
}

static void
gst_mfx_prime_buffer_proxy_finalize (GstMfxPrimeBufferProxy * proxy)
{
  if (g_va_get_surface_handle) {
    close (proxy->fd);
  }
  else {
    VAImage va_img;

    vaapi_image_get_image (proxy->image, &va_img);

    GST_MFX_DISPLAY_LOCK (proxy->display);
    vaReleaseBufferHandle (GST_MFX_DISPLAY_VADISPLAY (proxy->display), va_img.buf);
    GST_MFX_DISPLAY_UNLOCK (proxy->display);
  }

  vaapi_image_replace (&proxy->image, NULL);
  gst_mfx_surface_replace (&proxy->surface, NULL);
  gst_mfx_display_unref (proxy->display);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_prime_buffer_proxy_class (void)
{
  static const GstMfxMiniObjectClass GstMfxPrimeBufferProxyClass = {
    sizeof (GstMfxPrimeBufferProxy),
    (GDestroyNotify) gst_mfx_prime_buffer_proxy_finalize
  };
  return &GstMfxPrimeBufferProxyClass;
}

GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_new_from_surface (GstMfxSurface * surface)
{
  GstMfxPrimeBufferProxy *proxy;

  g_return_val_if_fail (surface != NULL, NULL);

  proxy = (GstMfxPrimeBufferProxy *)
      gst_mfx_mini_object_new0 (gst_mfx_prime_buffer_proxy_class ());
  if (!proxy)
    return NULL;

  proxy->surface = gst_mfx_surface_ref (surface);

  if (!gst_mfx_prime_buffer_proxy_acquire_handle (proxy))
    goto error_acquire_handle;

  return proxy;
  /* ERRORS */
error_acquire_handle:
  GST_ERROR ("failed to acquire the underlying PRIME buffer handle");
  gst_mfx_prime_buffer_proxy_unref (proxy);
  return NULL;
}

GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_ref (GstMfxPrimeBufferProxy * proxy)
{
  g_return_val_if_fail (proxy != NULL, NULL);

  return (GstMfxPrimeBufferProxy *) gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (proxy));
}

void
gst_mfx_prime_buffer_proxy_unref (GstMfxPrimeBufferProxy * proxy)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (proxy));
}

void
gst_mfx_prime_buffer_proxy_replace (GstMfxPrimeBufferProxy ** old_proxy_ptr,
    GstMfxPrimeBufferProxy * new_proxy)
{
  g_return_if_fail (old_proxy_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_proxy_ptr,
      GST_MFX_MINI_OBJECT (new_proxy));
}

guintptr
gst_mfx_prime_buffer_proxy_get_handle (GstMfxPrimeBufferProxy * proxy)
{
  g_return_val_if_fail (proxy != NULL, 0);

  return proxy->fd;
}

guint
gst_mfx_prime_buffer_proxy_get_size (GstMfxPrimeBufferProxy * proxy)
{
  g_return_val_if_fail (proxy != NULL, 0);

  return proxy->data_size;
}

VaapiImage *
gst_mfx_prime_buffer_proxy_get_vaapi_image (GstMfxPrimeBufferProxy * proxy)
{
  g_return_val_if_fail (proxy != NULL, 0);

  return vaapi_image_ref (proxy->image);
}
