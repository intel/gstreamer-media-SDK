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

#ifndef GST_MFX_DEVICE_H
#define GST_MFX_DEVICE_H

#include "sysdeps.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_DEVICE (gst_mfx_device_get_type ())
G_DECLARE_FINAL_TYPE(GstMfxDevice, gst_mfx_device, GST_MFX, DEVICE, GstObject)

#define GST_MFX_DEVICE(obj) ((GstMfxDevice *) (obj))

typedef struct _GstMfxDevice GstMfxDevice;

GstMfxDevice *
gst_mfx_device_new (GstMfxDevice * device, mfxSession session);

GstMfxDevice *
gst_mfx_device_ref(GstMfxDevice * device);

void
gst_mfx_device_unref(GstMfxDevice * device);

void
gst_mfx_device_replace(GstMfxDevice ** old_device_ptr,
  GstMfxDevice * new_device);

guintptr
gst_mfx_device_get_handle(GstMfxDevice * device);

IDXGIFactory2 *
gst_mfx_device_get_factory(GstMfxDevice * device);

ID3D11DeviceContext *
gst_mfx_device_get_d3d11_context(GstMfxDevice * device);

G_END_DECLS

#endif /* GST_MFX_DEVICE_H */