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

#ifndef GST_MFX_PRIME_BUFFER_PROXY_H
#define GST_MFX_PRIME_BUFFER_PROXY_H

#include "gstmfxutils_vaapi.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_PRIME_BUFFER_PROXY (gst_mfx_prime_buffer_proxy_get_type ())
G_DECLARE_FINAL_TYPE(GstMfxPrimeBufferProxy, gst_mfx_prime_buffer_proxy, GST_MFX, PRIME_BUFFER_PROXY, GstObject)

#define GST_MFX_PRIME_BUFFER_PROXY(obj) ((GstMfxPrimeBufferProxy *)(obj))

/**
 * GST_MFX_PRIME_BUFFER_PROXY_HANDLE:
 * @buf: a #GstMfxPrimeBufferProxy
 *
 * Macro that evaluates to the handle of the underlying VA buffer @buf
 */
#define GST_MFX_PRIME_BUFFER_PROXY_HANDLE(buf) \
  gst_mfx_prime_buffer_proxy_get_handle (GST_MFX_PRIME_BUFFER_PROXY (buf))

#define GST_MFX_PRIME_BUFFER_PROXY_SIZE(buf) \
  gst_mfx_prime_buffer_proxy_get_size (GST_MFX_PRIME_BUFFER_PROXY (buf))

typedef struct _GstMfxPrimeBufferProxy GstMfxPrimeBufferProxy;

GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_new_from_surface (GstMfxSurface * surface);

GstMfxPrimeBufferProxy *
gst_mfx_prime_buffer_proxy_ref (GstMfxPrimeBufferProxy * proxy);

void
gst_mfx_prime_buffer_proxy_unref (GstMfxPrimeBufferProxy * proxy);

void
gst_mfx_prime_buffer_proxy_replace (GstMfxPrimeBufferProxy ** old_proxy_ptr,
    GstMfxPrimeBufferProxy * new_proxy);

guintptr
gst_mfx_prime_buffer_proxy_get_handle (GstMfxPrimeBufferProxy * proxy);

guint
gst_mfx_prime_buffer_proxy_get_size (GstMfxPrimeBufferProxy * proxy);

VaapiImage *
gst_mfx_prime_buffer_proxy_get_vaapi_image (GstMfxPrimeBufferProxy *proxy);

G_END_DECLS

#endif /* GST_VAAPI_BUFFER_PROXY_H */
