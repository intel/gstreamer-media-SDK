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

#include "gstmfxd3d11device.h"
#include "gstmfxwindow_d3d11.h"
#include "gstmfxwindow_d3d11_priv.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxsurface_d3d11.h"
#include "video-format.h"

#define DEBUG 1
#include "gstmfxdebug.h"

static gboolean
gst_mfx_window_d3d11_render (GstMfxWindow * window, GstMfxSurface * surface,
  const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);
  GstMfxWindowD3D11Private *const priv2 =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  HRESULT hr = S_OK;
  gboolean ret = FALSE;

  if (!priv2->d3d11_video_context) {
    GST_ERROR("Failed to render surface : D3D11 context does not exist.");
    return FALSE;
  }

  ID3D11VideoProcessorInputView *input_view = NULL;
  D3D11_VIDEO_PROCESSOR_STREAM stream_data;
  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {
    .FourCC = 0,
    .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
    .Texture2D.MipSlice = 0,
    .Texture2D.ArraySlice = 0,
  };
  RECT rect = { 0 };
  rect.right = src_rect->width;
  rect.bottom = src_rect->height;

  if (!gst_mfx_surface_has_video_memory(surface)) {
    if (!priv2->mapped_surface) {
      priv2->mapped_surface =
          gst_mfx_surface_d3d11_new(priv->context, &priv2->info);
      if (!priv2->mapped_surface)
        return FALSE;

      gst_mfx_surface_d3d11_set_rw_flags(
        GST_MFX_SURFACE_D3D11(priv2->mapped_surface),
        MFX_SURFACE_WRITE);
    }

    if (!gst_mfx_surface_map(priv2->mapped_surface))
      return FALSE;

    memcpy(gst_mfx_surface_get_plane(priv2->mapped_surface, 0),
      gst_mfx_surface_get_plane(surface, 0),
      gst_mfx_surface_get_data_size(surface));

    gst_mfx_surface_unmap(priv2->mapped_surface);
  }

  hr = ID3D11VideoDevice_CreateVideoProcessorInputView(
          (ID3D11VideoDevice*)priv2->d3d11_video_device,
          (ID3D11Resource*)gst_mfx_surface_get_id(priv2->mapped_surface ?
            priv2->mapped_surface : surface),
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

  ID3D11VideoContext_VideoProcessorSetStreamSourceRect(
    priv2->d3d11_video_context, priv2->processor, 0, TRUE, &rect);

  if (priv2->keep_aspect) {
    D3D11_TEXTURE2D_DESC output_desc;
    RECT dest_rect = { 0 };
    gdouble src_ratio, window_ratio;

    ID3D11Texture2D_GetDesc(priv2->backbuffer_texture, &output_desc);

    src_ratio = (gdouble)src_rect->width / src_rect->height;
    window_ratio = (gdouble)priv->width / priv->height;

    if (src_ratio > window_ratio) {
      gdouble new_height =
          (gdouble)output_desc.Height * window_ratio / src_ratio;
      dest_rect.top = (output_desc.Height - new_height) / 2;
      dest_rect.bottom = new_height + dest_rect.top;
      dest_rect.left = 0;
      dest_rect.right = output_desc.Width;
    }
    else if (src_ratio < window_ratio) {
      gdouble new_width =
          (gdouble)output_desc.Width * src_ratio / window_ratio;
      dest_rect.top = 0;
      dest_rect.bottom = output_desc.Height;
      dest_rect.left = (output_desc.Width - new_width) / 2;
      dest_rect.right = dest_rect.left + new_width;
    }
    else {
      dest_rect.top = 0;
      dest_rect.bottom = output_desc.Height;
      dest_rect.left = 0;
      dest_rect.right = output_desc.Width;
    }

    ID3D11VideoContext_VideoProcessorSetStreamDestRect(
      priv2->d3d11_video_context, priv2->processor, 0, TRUE, &dest_rect);
  }

  ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(
    priv2->d3d11_video_context, priv2->processor,
    0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

  hr = ID3D11VideoContext_VideoProcessorBlt(priv2->d3d11_video_context,
    priv2->processor, priv2->output_view, 0, 1, &stream_data);
  if (SUCCEEDED(hr)) {
    IDXGISwapChain1_Present(priv2->dxgi_swapchain, 0, 0);
    ret = TRUE;
  }

  ID3D11VideoProcessorInputView_Release(input_view);

  return ret;
}

static gboolean
gst_mfx_window_d3d11_show(GstMfxWindow * window)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);

  ShowWindow((HWND)priv->handle, SW_SHOWDEFAULT);
  UpdateWindow((HWND)priv->handle);

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_hide (GstMfxWindow * window)
{
  GST_FIXME ("unimplemented GstMfxWindowD3D11::hide()");
  return TRUE;
}

static void
gst_mfx_window_d3d11_destroy (GObject * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);

  if (priv->output_view) {
    ID3D11VideoProcessorOutputView_Release(priv->output_view);
    priv->output_view = NULL;
  }
  if (priv->processor) {
    ID3D11VideoProcessor_Release(priv->processor);
    priv->processor = NULL;
  }
  if (priv->processor_enum) {
    ID3D11VideoProcessorEnumerator_Release(priv->processor_enum);
    priv->processor_enum = NULL;
  }
  if (priv->dxgi_swapchain) {
    IDXGISwapChain1_Release(priv->dxgi_swapchain);
    priv->dxgi_swapchain = NULL;
  }
  if (priv->d3d11_video_context) {
    ID3D11VideoContext_Release(priv->d3d11_video_context);
    priv->d3d11_video_context = NULL;
  }

  if (priv->d3d11_window.hInstance) {
    UnregisterClass(priv->d3d11_window.lpszClassName,
      priv->d3d11_window.hInstance);
    memset(&priv->d3d11_window, 0, sizeof(WNDCLASS));
  }

  PostMessage((HWND)GST_MFX_WINDOW_GET_PRIVATE(window)->handle, WM_QUIT, 0, 0);

  gst_mfx_surface_replace (&priv->mapped_surface, NULL);
  gst_mfx_d3d11_device_replace (&priv->device, NULL);
}

static gboolean
gst_mfx_window_d3d11_get_geometry (GstMfxWindow * window, gint * x, gint * y,
  guint * width, guint * height)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);
  RECT rect = { 0 };

  GetClientRect((HWND)priv->handle, &rect);

  *width = MAX(1, ABS(rect.right - rect.left));
  *height = MAX(1, ABS(rect.bottom - rect.top));

  return TRUE;
}

static LRESULT CALLBACK
WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  GstMfxWindow* window = (GstMfxWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

  if (window) {
    GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);

    switch (message) {
      case WM_SIZE:
        gst_mfx_window_d3d11_get_geometry(window, NULL, NULL,
          &priv->width, &priv->height);
        break;
      case WM_DESTROY:
        gst_mfx_context_lock(GST_MFX_WINDOW_GET_PRIVATE(window)->context);
        gst_mfx_window_d3d11_destroy (G_OBJECT (window));
        gst_mfx_context_unlock(GST_MFX_WINDOW_GET_PRIVATE(window)->context);
        PostQuitMessage(0);
        break;
      default:
        break;
    }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

static gboolean
gst_mfx_window_d3d11_create_output_view (GstMfxWindowD3D11 * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
    .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
    .Texture2D.MipSlice = 0,
  };

  HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(
                  (ID3D11VideoDevice*)priv->d3d11_video_device,
                  (ID3D11Resource*)priv->backbuffer_texture,
                  priv->processor_enum,
                  &output_view_desc,
                  (ID3D11VideoProcessorOutputView**)&priv->output_view);
  if (FAILED(hr))
    return FALSE;

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_create_video_processor (GstMfxWindowD3D11 * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
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
gst_mfx_window_d3d11_init_swap_chain (GstMfxWindowD3D11 * window)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);
  GstMfxWindowD3D11Private *const priv2 =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = { 0 };
  HRESULT hr = S_OK;

  swap_chain_desc.Width = 0;
  swap_chain_desc.Height = 0;

  swap_chain_desc.Format = gst_video_format_to_dxgi_format (
      GST_VIDEO_INFO_FORMAT(&priv2->info)
  );

  /* if chosen format isn't a possible render target, 
  we use a (A)RGB one the VideoProcessor can convert to. */
  if (swap_chain_desc.Format == DXGI_FORMAT_P010)
    swap_chain_desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
  else if (swap_chain_desc.Format == DXGI_FORMAT_NV12)
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.SampleDesc.Quality = 0;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.BufferCount = 2;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  //swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  hr = IDXGIFactory2_CreateSwapChainForHwnd(
          gst_mfx_d3d11_device_get_factory(priv2->device),
          (IUnknown*) gst_mfx_d3d11_device_get_handle(priv2->device),
          (HWND)priv->handle,
          &swap_chain_desc,
          NULL,
          NULL,
          &priv2->dxgi_swapchain);
  if (FAILED(hr))
    return FALSE;

  IDXGISwapChain1_GetBuffer(priv2->dxgi_swapchain, 0, &IID_ID3D11Texture2D,
    (void**)&priv2->backbuffer_texture);
  g_return_val_if_fail(priv2->backbuffer_texture != NULL, FALSE);

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_init_video_context (GstMfxWindowD3D11 * window)
{
  GstMfxWindowD3D11Private *const priv =
      GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  HRESULT hr = S_OK;

  hr = ID3D11Device_QueryInterface((ID3D11Device*)
    gst_mfx_d3d11_device_get_handle(priv->device),
    &IID_ID3D11VideoDevice, (void **)&priv->d3d11_video_device);
  if (FAILED(hr))
    return FALSE;

  hr = ID3D11DeviceContext_QueryInterface(
    gst_mfx_d3d11_device_get_d3d11_context(priv->device),
    &IID_ID3D11VideoContext, (void **)&priv->d3d11_video_context);
  if (FAILED(hr))
    return FALSE;

  return TRUE;
}

static gboolean
d3d11_create_render_context (GstMfxWindowD3D11 * window)
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
  gst_mfx_window_d3d11_destroy (G_OBJECT (window));
  return FALSE;
}

static gboolean
d3d11_create_window_internal (GstMfxWindowD3D11 * window)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE(window);
  GstMfxWindowD3D11Private *const priv2 =
    GST_MFX_WINDOW_D3D11_GET_PRIVATE(window);
  int width, height;
  int offx, offy;
  RECT rect;
  DWORD exstyle, style;
  int screenwidth;
  int screenheight;
  gchar *wnd_classname =
    g_strdup_printf("GstMfxWindowD3D11_%lu", GetCurrentThreadId());

  priv2->d3d11_window.lpfnWndProc = (WNDPROC)WindowProc;
  priv2->d3d11_window.hInstance = GetModuleHandle(NULL);
  priv2->d3d11_window.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  priv2->d3d11_window.hCursor = LoadCursor(NULL, IDC_ARROW);
  priv2->d3d11_window.lpszClassName = TEXT(wnd_classname);

  g_free(wnd_classname);

  if (RegisterClass(&priv2->d3d11_window) == 0) {
    GST_ERROR("Failed to register window class: %lu", GetLastError());
    return FALSE;
  }

  width = GetSystemMetrics(SM_CXSCREEN);
  height = GetSystemMetrics(SM_CYSCREEN);

  if (!priv->is_fullscreen) {
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
  }
  else {
    offx = 0;
    offy = 0;
    style = WS_POPUP;
  }
  exstyle = 0;
  priv->handle = (guintptr) CreateWindowEx(exstyle,
    priv2->d3d11_window.lpszClassName,
    TEXT("MFX D3D11 Internal Window"),
    style, offx, offy, width, height,
    NULL, NULL, priv2->d3d11_window.hInstance, NULL);

  if (!priv->handle) {
    GST_ERROR("Failed to create internal window: %lu",
      GetLastError());
    return FALSE;
  }

  SetWindowLongPtr((HWND)priv->handle, GWLP_USERDATA, (LONG_PTR)window);

  return TRUE;
}

typedef struct
{
  GstMfxWindowD3D11 *window;
  gboolean running;
} D3D11WindowThreadData;

static gpointer
d3d11_window_thread (D3D11WindowThreadData * data)
{
  GstMfxWindowD3D11 *window;
  MSG msg;

  g_return_val_if_fail (data != NULL, NULL);

  window = data->window;

  if (!d3d11_create_window_internal (window)) {
    GST_ERROR_OBJECT (window, "Failed to create D3D11 window");
    return NULL;
  }

  data->running = TRUE;

  while (GetMessage (&msg, NULL, 0, 0)) {
    if (msg.message == WM_QUIT)
      break;
    TranslateMessage (&msg);
    DispatchMessage (&msg);
  }

  GST_DEBUG_OBJECT (window, "Exiting D3D11 window thread: %p",
    g_thread_self ());

  return NULL;
}

static gboolean
d3d11_create_threaded_window (GstMfxWindowD3D11 * window)
{
  GThread *thread;
  D3D11WindowThreadData thread_data;
  gulong timeout_interval = 10000;
  gulong intervals = (10000000 / timeout_interval); // 10 secs - huge delay but coherent with sys/d3dvideosink/d3dhelpers.
  gulong i;

  thread_data.window = window;
  thread_data.running = FALSE;

  thread = g_thread_new ("gstmfxwindow-d3d11-window-thread",
    (GThreadFunc)d3d11_window_thread, &thread_data);
  if (!thread) {
    GST_ERROR ("Failed to created internal window thread");
    return FALSE;
  }

  /* Wait 10 seconds for window proc loop to start up */
  for (i = 0; thread_data.running == FALSE && i < intervals; i++) {
    g_usleep (timeout_interval);
  }

  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_create (GstMfxWindow * window,
  guint * width, guint * height)
{
  GstMfxWindowD3D11 * mfx_d3d11_window = GST_MFX_WINDOW_D3D11 (window);

  if (!d3d11_create_threaded_window (mfx_d3d11_window))
    return FALSE;

  if (!d3d11_create_render_context (mfx_d3d11_window))
    return FALSE;

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
  window_class->get_geometry = gst_mfx_window_d3d11_get_geometry;
}

static void
gst_mfx_window_d3d11_init (GstMfxWindowD3D11 * window)
{
}

GstMfxWindow *
gst_mfx_window_d3d11_new (GstMfxContext * context, GstVideoInfo * info,
  gboolean keep_aspect, gboolean fullscreen)
{
  GstMfxWindowD3D11 * window;

  g_return_val_if_fail(context != NULL, NULL);
  g_return_val_if_fail(info != NULL, NULL);

  window = g_object_new(GST_TYPE_MFX_WINDOW_D3D11, NULL);
  if (!window)
    return NULL;

  GST_MFX_WINDOW_D3D11_GET_PRIVATE(window)->device =
      gst_mfx_d3d11_device_ref(gst_mfx_context_get_device(context));
  GST_MFX_WINDOW_D3D11_GET_PRIVATE(window)->info = *info;
  GST_MFX_WINDOW_D3D11_GET_PRIVATE(window)->keep_aspect = keep_aspect;
  GST_MFX_WINDOW_GET_PRIVATE(window)->is_fullscreen = fullscreen;

  return gst_mfx_window_new_internal (GST_MFX_WINDOW(window), context,
    GST_MFX_ID_INVALID, 1, 1);
}
