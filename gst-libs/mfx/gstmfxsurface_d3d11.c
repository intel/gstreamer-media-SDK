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

#include "gstmfxsurface_d3d11.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_SURFACE_D3D11_CAST(obj) ((GstMfxSurfaceD3D11 *)(obj))

struct _GstMfxSurfaceD3D11
{
    /*< private > */
    GstMfxSurface parent_instance;
    GstMfxMemoryId *mid;
    GstMfxDevice *device;
};

G_DEFINE_TYPE(GstMfxSurfaceD3D11, gst_mfx_surface_d3d11, GST_TYPE_MFX_SURFACE);

static gboolean
gst_mfx_surface_d3d11_from_task(GstMfxSurface * surface,
    GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST(surface);

  d3d_surface->mid = gst_mfx_task_get_memory_id(task);
  if (!d3d_surface->mid)
    return FALSE;
  
  priv->surface.Data.MemId = d3d_surface->mid;
  priv->surface_id = d3d_surface->mid->mid;
  return TRUE;
}

static gboolean
gst_mfx_surface_d3d11_allocate(GstMfxSurface * surface, GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  
  priv->has_video_memory = TRUE;

  return gst_mfx_surface_d3d11_from_task(surface, task);
}

static void
gst_mfx_surface_d3d11_release(GstMfxSurface * surface)
{
  
}

static gboolean
gst_mfx_surface_d3d11_map (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST(surface);
  mfxFrameInfo *info = &priv->surface.Info;
  mfxFrameData *ptr = &priv->surface.Data;
  HRESULT hr = S_OK;
  D3D11_MAPPED_SUBRESOURCE locked_rect = { 0 };

  ID3D11Texture2D *texture = (ID3D11Texture2D *)d3d_surface->mid->mid;
  ID3D11Texture2D *stage = (ID3D11Texture2D *)d3d_surface->mid->mid_stage;
  ID3D11DeviceContext *d3d11_context =
      gst_mfx_device_get_d3d11_context(d3d_surface->device);

  // copy data only in case of user wants to read from stored surface
  if (d3d_surface->mid->rw & 0x1000)
    ID3D11DeviceContext_CopySubresourceRegion(d3d11_context, stage, 0, 0, 0, 0,
      texture, 0, NULL);

  do {
    hr = ID3D11DeviceContext_Map(d3d11_context,
      stage, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &locked_rect);
    if (S_OK != hr && DXGI_ERROR_WAS_STILL_DRAWING != hr)
      return FALSE;
  } while (DXGI_ERROR_WAS_STILL_DRAWING == hr);

  if (FAILED(hr))
    return FALSE;

  switch (info->FourCC) {
  case MFX_FOURCC_NV12:
    priv->pitches[0] = priv->pitches[1] = ptr->Pitch =
        (mfxU16)locked_rect.RowPitch;
    priv->planes[0] = ptr->Y = (mfxU8*)locked_rect.pData;
    priv->planes[1] = ptr->UV =
        (mfxU8*)locked_rect.pData + info->Height * locked_rect.RowPitch;
    break;
  case MFX_FOURCC_RGB4:
    priv->pitches[0] = ptr->Pitch = (mfxU16)locked_rect.RowPitch;
    priv->planes[0] = ptr->B = (mfxU8*)locked_rect.pData;
    ptr->G = ptr->B + 1;
    ptr->R = ptr->B + 2;
    ptr->A = ptr->B + 3;
    break;
  case MFX_FOURCC_YUY2:
    priv->pitches[0] = ptr->Pitch = (mfxU16)locked_rect.RowPitch;
    ptr->Y = (mfxU8*)locked_rect.pData;
    ptr->U = ptr->Y + 1;
    ptr->V = ptr->Y + 3;
    break;
  default:
    return FALSE;
  }

  return TRUE;
}

static void
gst_mfx_surface_d3d11_unmap(GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST(surface);
  mfxFrameData *ptr = &priv->surface.Data;

  ID3D11Texture2D *texture = (ID3D11Texture2D *)d3d_surface->mid->mid;
  ID3D11Texture2D *stage = (ID3D11Texture2D *)d3d_surface->mid->mid_stage;
  ID3D11DeviceContext *d3d11_context =
    gst_mfx_device_get_d3d11_context(d3d_surface->device);

  ID3D11DeviceContext_Unmap(d3d11_context, stage, 0);
  /* copy data only in case user wants to write to stored surface */
  if (d3d_surface->mid->rw & 0x2000)
    ID3D11DeviceContext_CopySubresourceRegion(d3d11_context, texture, 0, 0, 0, 0,
      stage, 0, NULL);

  if (ptr) {
    ptr->Pitch = 0;
    ptr->U = ptr->V = ptr->Y = 0;
    ptr->A = ptr->R = ptr->G = ptr->B = 0;
  }
}

void
gst_mfx_surface_d3d11_class_init(GstMfxSurfaceClass * klass)
{
  GstMfxSurfaceClass *const surface_class = GST_MFX_SURFACE_CLASS(klass);

  surface_class->allocate = gst_mfx_surface_d3d11_allocate;
  surface_class->release = gst_mfx_surface_d3d11_release;
  surface_class->map = gst_mfx_surface_d3d11_map;
  surface_class->unmap = gst_mfx_surface_d3d11_unmap;
}

static void
gst_mfx_surface_d3d11_init(GstMfxSurfaceD3D11 * surface)
{
}

GstMfxSurface *
gst_mfx_surface_d3d11_new_from_task(GstMfxSurfaceD3D11 * surface,
  GstMfxTask * task)
{
  GstMfxContext *context;

  g_return_val_if_fail(task != NULL, NULL);

  context = gst_mfx_task_get_context(task);
  surface->device = gst_mfx_context_get_device(context);
  gst_mfx_context_unref(context);

  return
    gst_mfx_surface_new_internal(GST_MFX_SURFACE(surface), NULL, task);
}
