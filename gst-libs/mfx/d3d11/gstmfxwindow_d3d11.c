/*
 *  Copyright (C)
 *    Author: Ishmael Visayana Sameen <ishmael1985@gmail.com>
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

#include "sysdeps.h"

#include "gstmfxwindow_d3d11.h"
#include "gstmfxwindow_d3d11_priv.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxsurface.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_WINDOW_D3D11_CAST(obj) \
	((GstMfxWindowD3D11 *)(obj))


static gboolean
gst_mfx_window_d3d11_render (GstMfxWindow * mfx_window,
    GstMfxSurface * surface,
    const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
  MSG msg;
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(mfx_window);
  GstMfxWindowD3D11Private *const priv2 = GST_MFX_WINDOW_D3D11_GET_PRIVATE(mfx_window);


  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
    // Translate the message and dispatch it to WindowProc()
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // If the message is WM_QUIT, exit
  if (msg.message == WM_QUIT) {
    gst_mfx_window_d3d11_destroy(mfx_window);
  }

  if (!gst_mfx_surface_has_video_memory(surface)) {
    D3D11_BOX destRegion;
    destRegion.left = 0;
    destRegion.right = priv->width;
    destRegion.top = 0;
    destRegion.bottom = priv->height;
    destRegion.front = 0;
    destRegion.back = 1;

    ID3D11DeviceContext_UpdateSubresource(priv2->d3d11_device_ctx, priv2->backbuffer_texture,
      0, &destRegion, gst_mfx_surface_get_plane(surface, 0),
      gst_mfx_surface_get_pitch(surface, 0), 0);//TODO: ensure RGB4
  }
  else {
    ID3D11DeviceContext_CopyResource(priv2->d3d11_device_ctx,
      priv2->backbuffer_texture, (ID3D11Texture2D*) gst_mfx_surface_get_id(surface));
  }

  IDXGISwapChain1_Present(priv2->dxgi_swapchain, 0, 0);

  return TRUE;
}

gst_mfx_window_d3d11_show(GstMfxWindow * mfx_window)
{
  GstMfxWindowD3D11Private *const priv2 = GST_MFX_WINDOW_D3D11_GET_PRIVATE(mfx_window);

  ShowWindow(priv2->hwnd, SW_SHOWDEFAULT);
  UpdateWindow(priv2->hwnd);

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_hide (GstMfxWindow * window)
{
  GST_FIXME ("unimplemented GstMfxWindowD3D11::hide()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_set_fullscreen (GstMfxWindow * window,
    gboolean fullscreen)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::set_fullscreen()");
  return TRUE;
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  GstMfxWindow* window = (GstMfxWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

  if (window)
  {
    switch (message)
    {
    case WM_DESTROY:
      gst_mfx_window_d3d11_destroy(window);
      break;
    }
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

static gboolean
gst_mfx_window_d3d11_create (GstMfxWindow * mfx_window,
  guint * width, guint * height)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::create()");
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(mfx_window);
  GstMfxWindowD3D11Private *const priv2 = GST_MFX_WINDOW_D3D11_GET_PRIVATE(mfx_window);


  {
#ifdef DEBUG
    UINT creationFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT creationFlags = 0;
#endif

    UINT adapter_idx = 0;
    HRESULT hr = CreateDXGIFactory(&IID_IDXGIFactory, (void**)(&priv2->dxgi_factory));

    while (IDXGIFactory2_EnumAdapters(priv2->dxgi_factory, adapter_idx, &priv2->dxgi_adapter) != DXGI_ERROR_NOT_FOUND)
    {
      DXGI_ADAPTER_DESC desc;
      IDXGIAdapter_GetDesc(priv2->dxgi_adapter, &desc);
      if (desc.VendorId == 0x8086) {//TODO: we can enable D3D11 rendering with frames on a separate device.
        D3D11CreateDevice(priv2->dxgi_adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, creationFlags, NULL,
          0, D3D11_SDK_VERSION, &priv2->d3d11_device, NULL, &priv2->d3d11_device_ctx);
        break;
      }
      adapter_idx++;
    }

    WNDCLASS d3d11_window = { 0 };

    d3d11_window.lpfnWndProc = (WNDPROC)WindowProc;
    d3d11_window.hInstance = GetModuleHandle(NULL);;
    d3d11_window.hCursor = LoadCursor(NULL, IDC_ARROW);
    d3d11_window.lpszClassName = "GstMfxWindowD3D11";

    if (!RegisterClass(&d3d11_window))
      return FALSE;

    priv2->hwnd = CreateWindowEx(0,
      d3d11_window.lpszClassName,
      "GstMfxWindow D3D11 Sink",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      NULL,
      NULL,
      d3d11_window.hInstance,
      NULL);

    g_return_val_if_fail(priv2->hwnd != NULL, FALSE);

    SetWindowLongPtr(priv2->hwnd, GWLP_USERDATA, (LONG_PTR)mfx_window);

    ShowWindow(priv2->hwnd, SW_SHOWDEFAULT);
    UpdateWindow(priv2->hwnd);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

    swapChainDesc.Width = *width;
    swapChainDesc.Height = *height; 

    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; //TODO: handle 10bpc with DXGI_FORMAT_R10G10B10A2_UNORM
    swapChainDesc.SampleDesc.Count = 1; 
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = IDXGIFactory2_CreateSwapChainForHwnd(priv2->dxgi_factory,
      priv2->d3d11_device,//TODO: check if should use D3D11Device or device context as priv->handle
      priv2->hwnd,
      &swapChainDesc,
      NULL,
      NULL,
      &priv2->dxgi_swapchain);
    g_return_val_if_fail(SUCCEEDED(hr), FALSE);

    IDXGISwapChain1_GetBuffer(priv2->dxgi_swapchain, 0, &IID_ID3D11Texture2D, &priv2->backbuffer_texture);
    g_return_val_if_fail(priv2->backbuffer_texture != NULL, FALSE);
  }

  //TODO: share device with input

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_destroy (GObject * window)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::destroy()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_resize (GstMfxWindow * window, guint width, guint height)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::resize()");
  return TRUE;
}

static void
gst_mfx_window_d3d11_class_init (GstMfxWindowD3D11Class * klass)
{
  GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS (klass);

  window_class->create = gst_mfx_window_d3d11_create;
  window_class->destroy = gst_mfx_window_d3d11_destroy;
  window_class->show = gst_mfx_window_d3d11_show;
  window_class->render = gst_mfx_window_d3d11_render;
  window_class->hide = gst_mfx_window_d3d11_hide;
  window_class->resize = gst_mfx_window_d3d11_resize;
  window_class->set_fullscreen = gst_mfx_window_d3d11_set_fullscreen;
}

static void
gst_mfx_window_d3d11_init(GstMfxWindowD3D11 * window)
{
}

GstMfxWindow *
gst_mfx_window_d3d11_new (GstMfxWindowD3D11 * window, guint width, guint height)
{
  GST_DEBUG ("new window, size %ux%u", width, height);

  return gst_mfx_window_new_internal (GST_MFX_WINDOW(window),
    GST_MFX_ID_INVALID, width, height);
}
