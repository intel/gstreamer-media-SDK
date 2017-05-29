/*
*  Copyright (C) 2017 Intel Corporation
*    Author: Xavier Hallade <xavier.hallade@intel.com>
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

#ifndef GST_MFX_WINDOW_D3D11_PRIV_H
#define GST_MFX_WINDOW_D3D11_PRIV_H

/*#define COBJMACROS 
#include <dxgi1_2.h>
#include <d3d11.h>*/

#include "gstmfxdevice.h"
#include "gstmfxwindow_d3d11.h"
#include "gstmfxwindow_priv.h"

G_BEGIN_DECLS

#define GST_MFX_IS_WINDOW_D3D11(window) \
  ((window) != NULL && \
  GST_MFX_WINDOW_TYPE (window) == GST_MFX_WINDOW_TYPE_D3D11)

#define GST_MFX_WINDOW_D3D11_CAST(window) \
  ((GstMfxWindowD3D11 *)(window))

#define GST_MFX_WINDOW_D3D11_GET_PRIVATE(window) \
  (&GST_MFX_WINDOW_D3D11_CAST(window)->priv)


typedef struct _GstMfxWindowD3D11Private   GstMfxWindowD3D11Private;

struct _GstMfxWindowD3D11Private
{
  GstMfxDevice *device;
  /*ID3D11Device *d3d11_device;
  ID3D11DeviceContext *d3d11_device_ctx;
  IDXGIFactory2 * dxgi_factory;
  IDXGIAdapter2 * dxgi_adapter;*/
  IDXGISwapChain1 * dxgi_swapchain;
  ID3D11Texture2D * backbuffer_texture;
  HWND hwnd;
};

/**
* GstMfxWindowD3D11:
*
* MFX/D3D11 window wrapper.
*/
struct _GstMfxWindowD3D11
{
  /*< private >*/
  GstMfxWindow parent_instance;

  GstMfxWindowD3D11Private priv;
};

G_DEFINE_TYPE(GstMfxWindowD3D11, gst_mfx_window_d3d11, GST_TYPE_MFX_WINDOW);

/**
* GstMfxWindowD3D11Class:
*
* MFX/D3D11 window wrapper clas.
*/
struct _GstMfxWindowD3d11Class
{
  /*< private >*/
  GstMfxWindowClass parent_class;
};

G_END_DECLS

#endif /* GST_MFX_WINDOW_D3D11_PRIV_H */