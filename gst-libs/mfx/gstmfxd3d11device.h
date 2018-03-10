/*
 *  Copyright (C) 2017
 *    Author: Ishmael Visayana Sameen <ishmael1985@gmail.com>
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

#ifndef GST_MFX_D3D11_DEVICE_H
#define GST_MFX_D3D11_DEVICE_H

#include "sysdeps.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_D3D11_DEVICE (gst_mfx_d3d11_device_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxD3D11Device, gst_mfx_d3d11_device, GST_MFX,
    D3D11_DEVICE, GstObject)
#define GST_MFX_DEVICE(obj) ((GstMfxD3D11Device *) (obj))

typedef struct _GstMfxD3D11Device GstMfxD3D11Device;

GstMfxD3D11Device *
gst_mfx_d3d11_device_new (mfxSession session);

GstMfxD3D11Device *
gst_mfx_d3d11_device_ref (GstMfxD3D11Device * device);

void
gst_mfx_d3d11_device_unref (GstMfxD3D11Device * device);

void
gst_mfx_d3d11_device_replace (GstMfxD3D11Device ** old_device_ptr,
    GstMfxD3D11Device * new_device);

guintptr
gst_mfx_d3d11_device_get_handle (GstMfxD3D11Device * device);

IDXGIFactory2 *
gst_mfx_d3d11_device_get_factory (GstMfxD3D11Device * device);

ID3D11DeviceContext *
gst_mfx_d3d11_device_get_d3d11_context (GstMfxD3D11Device * device);

G_END_DECLS
#endif /* GST_MFX_D3D11_DEVICE_H */
