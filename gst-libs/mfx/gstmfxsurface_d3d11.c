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
#include "gstmfxd3d11device.h"
#include "video-format.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_SURFACE_D3D11_CAST(obj) ((GstMfxSurfaceD3D11 *)(obj))

struct _GstMfxSurfaceD3D11
{
  /*< private > */
  GstMfxSurface parent_instance;
  GstMfxMemoryId *mid;
  GstMfxD3D11Device *device;
};

G_DEFINE_TYPE (GstMfxSurfaceD3D11, gst_mfx_surface_d3d11, GST_TYPE_MFX_SURFACE);

static gboolean
gst_mfx_surface_d3d11_from_task (GstMfxSurface * surface, GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST (surface);

  d3d_surface->mid = gst_mfx_task_get_memory_id (task);
  if (!d3d_surface->mid)
    return FALSE;

  priv->surface.Data.MemId = d3d_surface->mid;
  priv->surface_id = GST_MFX_ID (d3d_surface->mid->mid);
  return TRUE;
}

static gboolean
gst_mfx_surface_d3d11_allocate (GstMfxSurface * surface, GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST (surface);

  priv->has_video_memory = TRUE;

  if (task) {
    priv->context = gst_mfx_task_get_context (task);
    return gst_mfx_surface_d3d11_from_task (surface, task);
  } else {
    mfxFrameInfo *frame_info = &priv->surface.Info;
    ID3D11Device *d3d11_device;
    ID3D11Texture2D *texture;
    DXGI_FORMAT format;
    D3D11_TEXTURE2D_DESC desc = { 0 };
    HRESULT hr = S_OK;

    d3d_surface->device = gst_mfx_context_get_device (priv->context);
    d3d11_device = (ID3D11Device *)
        gst_mfx_d3d11_device_get_handle (d3d_surface->device);

    format = gst_mfx_fourcc_to_dxgi_format (frame_info->FourCC);
    if (DXGI_FORMAT_UNKNOWN == format)
      return FALSE;

    /* Create surface textures */
    desc.Width = frame_info->Width;
    desc.Height = frame_info->Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;         // number of subresources is 1 in this case
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.MiscFlags = 0;

    hr = ID3D11Device_CreateTexture2D ((ID3D11Device *) d3d11_device, &desc,
        NULL, &texture);
    if (FAILED (hr))
      return FALSE;

    priv->mem_id.mid = texture;
    priv->mem_id.info = frame_info;

    /* Create surface staging texture */
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    hr = ID3D11Device_CreateTexture2D ((ID3D11Device *) d3d11_device, &desc,
        NULL, &texture);
    if (FAILED (hr))
      return FALSE;

    priv->mem_id.mid_stage = texture;

    d3d_surface->mid = priv->surface.Data.MemId = &priv->mem_id;
    priv->surface_id = GST_MFX_ID (priv->mem_id.mid);
  }
  return TRUE;
}

static void
gst_mfx_surface_d3d11_release (GObject * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);

  if (!priv->task) {
    ID3D11Texture2D *surface = (ID3D11Texture2D *) priv->mem_id.mid;
    ID3D11Texture2D *stage = (ID3D11Texture2D *) priv->mem_id.mid_stage;

    if (surface)
      ID3D11Texture2D_Release (surface);
    if (stage)
      ID3D11Texture2D_Release (stage);
  }
}

static gboolean
gst_mfx_surface_d3d11_map (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST (surface);
  mfxFrameInfo *info = &priv->surface.Info;
  mfxFrameData *ptr = &priv->surface.Data;
  HRESULT hr = S_OK;
  D3D11_MAPPED_SUBRESOURCE locked_rect = { 0 };

  ID3D11Texture2D *texture = (ID3D11Texture2D *) d3d_surface->mid->mid;
  ID3D11Texture2D *stage = (ID3D11Texture2D *) d3d_surface->mid->mid_stage;
  ID3D11DeviceContext *d3d11_context =
      gst_mfx_d3d11_device_get_d3d11_context (d3d_surface->device);

  /* copy data only in case of user wants to read from stored surface */
  if (d3d_surface->mid->rw & MFX_SURFACE_READ)
    ID3D11DeviceContext_CopyResource (d3d11_context,
        (ID3D11Resource *) stage, (ID3D11Resource *) texture);

  do {
    hr = ID3D11DeviceContext_Map (d3d11_context, (ID3D11Resource *) stage,
        0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &locked_rect);
    if (S_OK != hr && DXGI_ERROR_WAS_STILL_DRAWING != hr)
      return FALSE;
  } while (DXGI_ERROR_WAS_STILL_DRAWING == hr);

  if (FAILED (hr))
    return FALSE;

  switch (info->FourCC) {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
      priv->pitches[0] = priv->pitches[1] = ptr->Pitch =
          (mfxU16) locked_rect.RowPitch;
      priv->planes[0] = ptr->Y = (mfxU8 *) locked_rect.pData;
      priv->planes[1] = ptr->UV =
          (mfxU8 *) locked_rect.pData + info->Height * locked_rect.RowPitch;
      break;
    case MFX_FOURCC_RGB4:
      priv->pitches[0] = ptr->Pitch = (mfxU16) locked_rect.RowPitch;
      priv->planes[0] = ptr->B = (mfxU8 *) locked_rect.pData;
      ptr->G = ptr->B + 1;
      ptr->R = ptr->B + 2;
      ptr->A = ptr->B + 3;
      break;
    case MFX_FOURCC_YUY2:
      priv->pitches[0] = ptr->Pitch = (mfxU16) locked_rect.RowPitch;
      ptr->Y = (mfxU8 *) locked_rect.pData;
      ptr->U = ptr->Y + 1;
      ptr->V = ptr->Y + 3;
      break;
    default:
      return FALSE;
  }

  return TRUE;
}

static void
gst_mfx_surface_d3d11_unmap (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceD3D11 *const d3d_surface = GST_MFX_SURFACE_D3D11_CAST (surface);
  mfxFrameData *ptr = &priv->surface.Data;

  ID3D11Texture2D *texture = (ID3D11Texture2D *) d3d_surface->mid->mid;
  ID3D11Texture2D *stage = (ID3D11Texture2D *) d3d_surface->mid->mid_stage;
  ID3D11DeviceContext *d3d11_context =
      gst_mfx_d3d11_device_get_d3d11_context (d3d_surface->device);

  ID3D11DeviceContext_Unmap (d3d11_context, (ID3D11Resource *) stage, 0);
  /* copy data only in case user wants to write to stored surface */
  if (d3d_surface->mid->rw & MFX_SURFACE_WRITE)
    ID3D11DeviceContext_CopyResource (d3d11_context,
        (ID3D11Resource *) texture, (ID3D11Resource *) stage);

  if (ptr) {
    ptr->Pitch = 0;
    ptr->U = ptr->V = ptr->Y = 0;
    ptr->A = ptr->R = ptr->G = ptr->B = 0;
  }
}

void
gst_mfx_surface_d3d11_class_init (GstMfxSurfaceD3D11Class * klass)
{
  GstMfxSurfaceClass *const surface_class = GST_MFX_SURFACE_CLASS (klass);

  surface_class->allocate = gst_mfx_surface_d3d11_allocate;
  surface_class->release = gst_mfx_surface_d3d11_release;
  surface_class->map = gst_mfx_surface_d3d11_map;
  surface_class->unmap = gst_mfx_surface_d3d11_unmap;
}

static void
gst_mfx_surface_d3d11_init (GstMfxSurfaceD3D11 * surface)
{
}

GstMfxSurface *
gst_mfx_surface_d3d11_new (GstMfxContext * context, const GstVideoInfo * info)
{
  GstMfxSurfaceD3D11 *surface;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (info != NULL, NULL);

  surface = g_object_new (GST_TYPE_MFX_SURFACE_D3D11, NULL);
  if (!surface)
    return NULL;

  return
      gst_mfx_surface_new_internal (GST_MFX_SURFACE (surface),
      context, info, NULL);
}

GstMfxSurface *
gst_mfx_surface_d3d11_new_from_task (GstMfxTask * task)
{
  GstMfxSurfaceD3D11 *surface;
  GstMfxContext *context;

  g_return_val_if_fail (task != NULL, NULL);

  surface = g_object_new (GST_TYPE_MFX_SURFACE_D3D11, NULL);
  if (!surface)
    return NULL;

  context = gst_mfx_task_get_context (task);
  surface->device = gst_mfx_context_get_device (context);
  gst_mfx_context_unref (context);

  return
      gst_mfx_surface_new_internal (GST_MFX_SURFACE (surface),
      context, NULL, task);
}

void
gst_mfx_surface_d3d11_set_rw_flags (GstMfxSurfaceD3D11 * surface, guint flags)
{
  g_return_if_fail (surface != NULL);
  surface->mid->rw = flags;
}
