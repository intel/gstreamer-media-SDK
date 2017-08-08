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

#include "sysdeps.h"
#include "gstmfxd3d11device.h"

struct _GstMfxD3D11Device
{
  GstObject parent_instance;

  ID3D11Device *d3d11_device;
  ID3D11DeviceContext *d3d11_context;
  IDXGIFactory2 *dxgi_factory;
  IDXGIAdapter2 *dxgi_adapter;
};

G_DEFINE_TYPE(GstMfxD3D11Device, gst_mfx_d3d11_device, GST_TYPE_OBJECT);

struct {
  mfxIMPL impl;       // actual implementation
  mfxU32  adapter_id;  // device adapter number
} impl_types[] = {
  { MFX_IMPL_HARDWARE, 0 },
  { MFX_IMPL_HARDWARE2, 1 },
  { MFX_IMPL_HARDWARE3, 2 },
  { MFX_IMPL_HARDWARE4, 3 }
};

static void
gst_mfx_d3d11_device_finalize (GObject * object)
{
  GstMfxD3D11Device *device = GST_MFX_D3D11_DEVICE (object);

  if (device->dxgi_adapter) {
    IDXGIAdapter_Release (device->dxgi_adapter);
    device->dxgi_adapter = NULL;
  }
  if (device->dxgi_factory) {
    IDXGIFactory2_Release (device->dxgi_factory);
    device->dxgi_factory = NULL;
  }
  if (device->d3d11_context) {
    ID3D11DeviceContext_Release (device->d3d11_context);
    device->d3d11_context = NULL;
  }
  if (device->d3d11_device) {
    ID3D11Device_Release (device->d3d11_device);
    device->d3d11_device = NULL;
  }
}

static void
gst_mfx_d3d11_device_class_init (GstMfxD3D11DeviceClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gst_mfx_d3d11_device_finalize;
}

static void
gst_mfx_d3d11_device_init(GstMfxD3D11Device * device)
{
  device->d3d11_device = NULL;
}

static gboolean
get_intel_device_adapter (GstMfxD3D11Device * device, mfxSession session)
{
  mfxU32  adapter_idx = 0;
  mfxU8 i; 
  mfxIMPL impl;
  HRESULT hr = S_OK;

  MFXQueryIMPL(session, &impl);

  /* Extract Media SDK base implementation type */
  mfxIMPL base_impl = MFX_IMPL_BASETYPE (impl);

  /* get corresponding adapter number */
  for (i = 0; i < sizeof(impl_types) / sizeof(impl_types[0]); i++) {
    if (impl_types[i].impl == base_impl) {
      adapter_idx = impl_types[i].adapter_id;
      break;
    }
  }

  hr = CreateDXGIFactory (&IID_IDXGIFactory2, (void**)(&device->dxgi_factory));
  if (FAILED(hr))
    return FALSE;

  hr = IDXGIFactory2_EnumAdapters (device->dxgi_factory, adapter_idx,
    (IDXGIAdapter**)&device->dxgi_adapter);
  if (FAILED(hr))
    return FALSE;
  
  return TRUE;
}

static gboolean
gst_mfx_d3d11_device_create (GstMfxD3D11Device * device, mfxSession session)
{
  ID3D10Multithread *multithread_ptr = NULL;
  static D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0
  };
  D3D_FEATURE_LEVEL feature_level_out;
  HRESULT hr = S_OK;
#ifdef DEBUG
  UINT dx_flags = D3D11_CREATE_DEVICE_DEBUG;
#else
  UINT dx_flags = 0;
#endif

  if (!get_intel_device_adapter(device, session))
    return FALSE;

  hr = D3D11CreateDevice ((IDXGIAdapter*) device->dxgi_adapter,
    D3D_DRIVER_TYPE_UNKNOWN,
    NULL,
    dx_flags,
    feature_levels,
    (sizeof (feature_levels) / sizeof (feature_levels[0])),
    D3D11_SDK_VERSION,
    &device->d3d11_device,
    &feature_level_out,
    &device->d3d11_context);
  if (FAILED(hr))
    return FALSE;

  hr = ID3D11Device_QueryInterface (device->d3d11_device,
        &IID_ID3D10Multithread, (void **)&multithread_ptr);
  if (SUCCEEDED(hr)) {
    ID3D10Multithread_SetMultithreadProtected (multithread_ptr, TRUE);
    ID3D10Multithread_Release (multithread_ptr);
  }
  else {
    return FALSE;
  }

  return TRUE;
}

GstMfxD3D11Device *
gst_mfx_d3d11_device_new (mfxSession session)
{
  GstMfxD3D11Device *device;

  g_return_val_if_fail (session != NULL, NULL);

  device = g_object_new (GST_TYPE_MFX_D3D11_DEVICE, NULL);
  if (!device)
    return NULL;

  if (!gst_mfx_d3d11_device_create (device, session))
    goto error;
  return device;

error:
  gst_mfx_d3d11_device_unref (device);
  return NULL;
}

GstMfxD3D11Device *
gst_mfx_d3d11_device_ref (GstMfxD3D11Device * device)
{
  g_return_val_if_fail (device != NULL, NULL);

  return gst_object_ref (GST_OBJECT(device));
}

void
gst_mfx_d3d11_device_unref (GstMfxD3D11Device * device)
{
  gst_object_unref (GST_OBJECT(device));
}

void
gst_mfx_d3d11_device_replace (GstMfxD3D11Device ** old_device_ptr,
  GstMfxD3D11Device * new_device)
{
  g_return_if_fail (old_device_ptr != NULL);

  gst_object_replace ((GstObject **)old_device_ptr,
    GST_OBJECT (new_device));
}

guintptr
gst_mfx_d3d11_device_get_handle (GstMfxD3D11Device * device)
{
  g_return_val_if_fail (device != NULL, (guintptr)NULL);

  return (guintptr) device->d3d11_device;
}

IDXGIFactory2 *
gst_mfx_d3d11_device_get_factory (GstMfxD3D11Device * device)
{
  g_return_val_if_fail (device != NULL, NULL);

  return device->dxgi_factory;
}

ID3D11DeviceContext *
gst_mfx_d3d11_device_get_d3d11_context (GstMfxD3D11Device * device)
{
  g_return_val_if_fail (device != NULL, NULL);

  return device->d3d11_context;
}
