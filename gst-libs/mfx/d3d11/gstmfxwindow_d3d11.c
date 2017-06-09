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

#include "gstmfxdevice.h"
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
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(mfx_window);
  GstMfxWindowD3D11Private *const priv2 = GST_MFX_WINDOW_D3D11_GET_PRIVATE(mfx_window);
  HRESULT hr = S_OK;
  MSG msg;

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

  if (gst_mfx_surface_has_video_memory(surface)) {
    ID3D11VideoProcessorInputView *input_view = NULL;
    D3D11_VIDEO_PROCESSOR_STREAM stream_data;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {
      .FourCC = 0,
      .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
      .Texture2D.MipSlice = 0,
      .Texture2D.ArraySlice = 0,
    };
    RECT rect = { 0 };
    rect.right = GST_MFX_SURFACE_FRAME_SURFACE(surface)->Info.CropW;
    rect.bottom = GST_MFX_SURFACE_FRAME_SURFACE(surface)->Info.CropH;

    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(
            (ID3D11VideoDevice*)priv2->d3d11_video_device,
            (ID3D11Texture2D*)gst_mfx_surface_get_id(surface),
            priv2->processor_enum,
            &input_view_desc,
            &input_view);
    if (FAILED(hr))
      return FALSE;

    stream_data.Enable = TRUE;
    stream_data.OutputIndex = 0;
    stream_data.InputFrameOrField = 0;
    stream_data.PastFrames = 0;
    stream_data.FutureFrames = 0;
    stream_data.ppPastSurfaces = NULL;
    stream_data.ppFutureSurfaces = NULL;
    stream_data.pInputSurface = input_view;
    stream_data.ppPastSurfacesRight = NULL;
    stream_data.ppFutureSurfacesRight = NULL;
    stream_data.pInputSurfaceRight = NULL;

    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(priv2->d3d11_video_context,
      priv2->processor, 0, TRUE, &rect);

    if (priv2->keep_aspect) {
      D3D11_TEXTURE2D_DESC outputDesc;
      RECT dest_rect = { 0 };
      gdouble src_ratio, window_ratio;

      ID3D11Texture2D_GetDesc(priv2->backbuffer_texture, &outputDesc);

      src_ratio = (gdouble)src_rect->width / src_rect->height;
      window_ratio = (gdouble)priv->width / priv->height;

      if (src_ratio > window_ratio) {
        gdouble new_height = (gdouble)outputDesc.Height * window_ratio / src_ratio;
        dest_rect.top = (outputDesc.Height - new_height) / 2;
        dest_rect.bottom = new_height + dest_rect.top;
        dest_rect.left = 0;
        dest_rect.right = outputDesc.Width;
      }
      else if (src_ratio < window_ratio) {
        gdouble new_width = (gdouble)outputDesc.Width * src_ratio / window_ratio;
        dest_rect.top = 0;
        dest_rect.bottom = outputDesc.Height;
        dest_rect.left = (outputDesc.Width - new_width) / 2;
        dest_rect.right = dest_rect.left + new_width;
      }
      else {
        dest_rect.top = 0;
        dest_rect.bottom = outputDesc.Height;
        dest_rect.left = 0;
        dest_rect.right = outputDesc.Width;
      }

      ID3D11VideoContext_VideoProcessorSetStreamDestRect(priv2->d3d11_video_context,
        priv2->processor, 0, TRUE, &dest_rect);
    }

    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(priv2->d3d11_video_context,
      priv2->processor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

    hr = ID3D11VideoContext_VideoProcessorBlt(priv2->d3d11_video_context,
      priv2->processor, priv2->output_view, 0, 1, &stream_data);
    if (FAILED(hr))
      return FALSE;
  }
  else {
    D3D11_BOX destRegion;
    destRegion.left = 0;
    destRegion.right = priv->width;
    destRegion.top = 0;
    destRegion.bottom = priv->height;
    destRegion.front = 0;
    destRegion.back = 1;

    ID3D11DeviceContext_UpdateSubresource(
      gst_mfx_device_get_d3d11_context(priv2->device),
      priv2->backbuffer_texture,
      0, &destRegion, gst_mfx_surface_get_plane(surface, 0),
      gst_mfx_surface_get_pitch(surface, 0), 0);//TODO: ensure RGB4

      /*ID3D11DeviceContext_UpdateSubresource(priv2->d3d11_device_ctx, priv2->backbuffer_texture,
      0, &destRegion, gst_mfx_surface_get_plane(surface, 0),
      gst_mfx_surface_get_pitch(surface, 0), 0);*/
  }

  IDXGISwapChain1_Present(priv2->dxgi_swapchain, 0, 0);

  return TRUE;
}

gst_mfx_window_d3d11_show(GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);

  ShowWindow(priv->hwnd, SW_SHOWDEFAULT);
  UpdateWindow(priv->hwnd);

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

  if (window) {
    switch (message) {
    case WM_DESTROY:
      gst_mfx_window_d3d11_destroy(window);
      break;
    case WM_SIZE:
      gst_mfx_window_d3d11_resize(window);
      break;
    }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

static gboolean
gst_mfx_window_d3d11_create_output_view(GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv = GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
    .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
    .Texture2D.MipSlice = 0,
  };

  HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(
                  (ID3D11VideoDevice*)priv->d3d11_video_device,
                  priv->backbuffer_texture,
                  priv->processor_enum,
                  &output_view_desc,
                  (ID3D11VideoProcessorOutputView**)&priv->output_view);
  if (FAILED(hr))
    return FALSE;

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_create_video_processor(GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv = GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;
  HRESULT hr = S_OK;

  content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  content_desc.InputFrameRate.Numerator = priv->info.fps_n;
  content_desc.InputFrameRate.Denominator = priv->info.fps_d;
  content_desc.InputWidth = priv->info.width;
  content_desc.InputHeight = priv->info.height;
  content_desc.OutputWidth = priv->info.width;
  content_desc.OutputHeight = priv->info.height;
  content_desc.OutputFrameRate.Numerator = priv->info.fps_n;
  content_desc.OutputFrameRate.Denominator = priv->info.fps_d;

  content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(
          (ID3D11VideoDevice*)priv->d3d11_video_device,
          &content_desc, &priv->processor_enum);
  if (FAILED(hr))
    return FALSE;

  hr = ID3D11VideoDevice_CreateVideoProcessor(
          (ID3D11VideoDevice*)priv->d3d11_video_device,
          priv->processor_enum, 0, &priv->processor);
  if (FAILED(hr))
    return FALSE;

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_init_swap_chain(GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv = GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = { 0 };
  HRESULT hr = S_OK;

  swap_chain_desc.Width = 0;
  swap_chain_desc.Height = 0;

  swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; //TODO: handle 10bpc with DXGI_FORMAT_R10G10B10A2_UNORM
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.SampleDesc.Quality = 0;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.BufferCount = 2;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  //swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  hr = IDXGIFactory2_CreateSwapChainForHwnd(
    gst_mfx_device_get_factory(priv->device),
    gst_mfx_device_get_handle(priv->device),
    priv->hwnd,
    &swap_chain_desc,
    NULL,
    NULL,
    &priv->dxgi_swapchain);
  if (FAILED(hr))
    return FALSE;

  IDXGISwapChain1_GetBuffer(priv->dxgi_swapchain, 0, &IID_ID3D11Texture2D,
    &priv->backbuffer_texture);
  g_return_val_if_fail(priv->backbuffer_texture != NULL, FALSE);

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_init_video_context(GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  HRESULT hr = S_OK;

  hr = ID3D11Device_QueryInterface((ID3D11Device*)
    gst_mfx_device_get_handle(priv->device),
    &IID_ID3D11VideoDevice, (void **)&priv->d3d11_video_device);
  if (FAILED(hr))
    return FALSE;

  hr = ID3D11DeviceContext_QueryInterface(
    gst_mfx_device_get_d3d11_context(priv->device),
    &IID_ID3D11VideoContext, (void **)&priv->d3d11_video_context);
  if (FAILED(hr))
    return FALSE;

  return TRUE;
}

static gboolean
d3d11_create_render_context(GstMfxWindow * window)
{
  if (!gst_mfx_window_d3d11_init_video_context(window))
    goto error;
  if (!gst_mfx_window_d3d11_init_swap_chain(window))
    goto error;
  if (!gst_mfx_window_d3d11_create_video_processor(window))
    goto error;
  if (!gst_mfx_window_d3d11_create_output_view(window))
    goto error;
  return TRUE;

error:
  gst_mfx_window_d3d11_destroy(window);
  return FALSE;
}

static gboolean
d3d11_create_window(GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  int width, height;
  int offx, offy;
  RECT rect;
  DWORD exstyle, style;
  int screenwidth;
  int screenheight;

  WNDCLASS d3d11_window = { 0 };

  d3d11_window.lpfnWndProc = (WNDPROC)WindowProc;
  d3d11_window.hInstance = GetModuleHandle(NULL);;
  d3d11_window.hCursor = LoadCursor(NULL, IDC_ARROW);
  d3d11_window.lpszClassName = "GstMfxWindowD3D11";

  if (!RegisterClass(&d3d11_window))
    return FALSE;

  width = GetSystemMetrics(SM_CXSCREEN);
  height = GetSystemMetrics(SM_CYSCREEN);

  SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
  screenwidth = rect.right - rect.left;
  screenheight = rect.bottom - rect.top;
  offx = rect.left;
  offy = rect.top;

  /* Make it fit into the screen without changing the aspect ratio. */
  if (width > screenwidth) {
    double ratio = (double)screenwidth / (double)width;
    width = screenwidth;
    height = (int)(height * ratio);
  }

  if (height > screenheight) {
    double ratio = (double)screenheight / (double)height;
    height = screenheight;
    width = (int)(width * ratio);
  }

  style = WS_OVERLAPPEDWINDOW;  /* Normal top-level window */
  exstyle = 0;
  priv->hwnd = CreateWindowEx(exstyle,
    d3d11_window.lpszClassName,
    TEXT("MFX D3D11 Renderer"),
    style, offx, offy, width, height,
    NULL, NULL, d3d11_window.hInstance, NULL);

  if (!priv->hwnd) {
    GST_ERROR("Failed to create internal window: %lu",
      GetLastError());
    return FALSE;
  }

  SetWindowLongPtr(priv->hwnd, GWLP_USERDATA, (LONG_PTR)window);

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_create (GstMfxWindow * window,
  guint * width, guint * height)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);
  GstMfxWindowD3D11Private *const priv2 =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  RECT rect;

  if (!d3d11_create_window(window))
    return FALSE;

  if (!d3d11_create_render_context(window))
    return FALSE;

  GetClientRect(priv2->hwnd, &rect);

  priv->width = *width = MAX(1, ABS(rect.right - rect.left));
  priv->height = *height = MAX(1, ABS(rect.bottom - rect.top));

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_destroy (GstMfxWindow * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);

  if (priv->d3d11_video_context)
    ID3D11VideoContext_Release(priv->d3d11_video_context);
  if (priv->processor)
    ID3D11VideoProcessor_Release(priv->processor);
  if (priv->processor_enum)
    ID3D11VideoProcessorEnumerator_Release(priv->processor_enum);

  gst_mfx_device_replace(&priv->device, NULL);
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_resize (GstMfxWindow * window, guint width, guint height)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);
  GstMfxWindowD3D11Private *const priv2 =
    GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  RECT rect = { 0 };

  GetClientRect(priv2->hwnd, &rect);

  priv->width = MAX(1, ABS(rect.right - rect.left));
  priv->height = MAX(1, ABS(rect.bottom - rect.top));

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
gst_mfx_window_d3d11_new (GstMfxWindowD3D11 * window, GstMfxContext * context,
  GstVideoInfo * info, gboolean keep_aspect)
{
  GST_MFX_WINDOW_D3D11_GET_PRIVATE(window)->device =
      gst_mfx_context_get_device(context);
  GST_MFX_WINDOW_D3D11_GET_PRIVATE(window)->info = *info;
  GST_MFX_WINDOW_D3D11_GET_PRIVATE(window)->keep_aspect = keep_aspect;

  return gst_mfx_window_new_internal (GST_MFX_WINDOW(window), context,
    //GST_MFX_ID_INVALID, 
    1, 1);
}
