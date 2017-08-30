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

#include "gstmfxallocator.h"
#include "gstmfxtask.h"
#include "gstmfxtask_priv.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxcontext.h"
#include "video-format.h"

static gint
find_response (gconstpointer response_data, gconstpointer response)
{
  ResponseData *_response_data = (ResponseData *) response_data;
  mfxFrameAllocResponse *_response = (mfxFrameAllocResponse *) response;

  return _response_data ?
      (GstMfxMemoryId **) _response->mids != _response_data->mids : -1;
}

static void
free_memory_ids (ResponseData * response_data)
{
  guint i;

  if (response_data->mids) {
    for (i = 0; i < response_data->num_surfaces; i++) {
      if (response_data->mids[i]) {
        GstMfxMemoryId *mem_id = (GstMfxMemoryId *) response_data->mids[i];
        ID3D11Texture2D *surface = (ID3D11Texture2D *) mem_id->mid;
        ID3D11Texture2D *stage = (ID3D11Texture2D *) mem_id->mid_stage;

        if (surface)
          ID3D11Texture2D_Release (surface);
        if (stage)
          ID3D11Texture2D_Release (stage);

        g_slice_free (GstMfxMemoryId, mem_id);
      }
    }
    g_slice_free1 (response_data->num_surfaces * sizeof (GstMfxMemoryId *),
        response_data->mids);
    response_data->mids = NULL;
  }
}

mfxStatus
gst_mfx_task_frame_alloc (mfxHDL pthis, mfxFrameAllocRequest * request,
    mfxFrameAllocResponse * response)
{
  GstMfxTask *task =
      gst_mfx_task_aggregator_get_current_task (GST_MFX_TASK_AGGREGATOR
      (pthis));
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE (task);
  ID3D11Device *d3d11_device = (ID3D11Device *)
      gst_mfx_d3d11_device_get_handle (gst_mfx_context_get_device
      (priv->context));
  HRESULT hr = S_OK;
  ResponseData *response_data;
  guint i;

  if (priv->saved_responses
      && gst_mfx_task_has_type (task, GST_MFX_TASK_DECODER)
      && (request->Type & MFX_MEMTYPE_INTERNAL_FRAME) == 0) {
    GList *l = g_list_last (priv->saved_responses);
    if (l) {
      response_data = l->data;
      *response = response_data->response;
      return MFX_ERR_NONE;
    }
  }

  response_data = g_malloc0 (sizeof (ResponseData));
  response_data->frame_info = request->Info;

  if (request->Type & MFX_MEMTYPE_INTERNAL_FRAME)
    response_data->num_surfaces = request->NumFrameSuggested;
  else
    response_data->num_surfaces = priv->request.NumFrameSuggested;

  /* Allocate custom container to keep texture and stage buffers for each surface.
   * Container also stores the intended read and/or write operation. */
  response_data->mids =
      g_slice_alloc0 (response_data->num_surfaces * sizeof (GstMfxMemoryId *));
  if (!response_data->mids)
    return MFX_ERR_MEMORY_ALLOC;

  for (i = 0; i < response_data->num_surfaces; i++) {
    response_data->mids[i] = g_slice_new0 (GstMfxMemoryId);
    if (!response_data->mids[i])
      goto error;
  }

  if (MFX_FOURCC_P8 == request->Info.FourCC) {
    D3D11_BUFFER_DESC desc = { 0 };

    desc.ByteWidth = request->Info.Width * request->Info.Height;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    ID3D11Buffer *buffer = 0;
    hr = ID3D11Device_CreateBuffer ((ID3D11Device *) d3d11_device, &desc, 0,
        &buffer);
    if (FAILED (hr))
      goto error;

    response_data->mids[0]->mid = (ID3D11Texture2D *) (buffer);
    response_data->mids[0]->info = &response_data->frame_info;
  } else {
    ID3D11Texture2D *texture;
    DXGI_FORMAT format;
    D3D11_TEXTURE2D_DESC desc = { 0 };

    format = gst_mfx_fourcc_to_dxgi_format (request->Info.FourCC);
    if (DXGI_FORMAT_UNKNOWN == format)
      goto error;

    desc.Width = request->Info.Width;
    desc.Height = request->Info.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;         // number of subresources is 1 in this case
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;        // required by dxgl interop. TODO: use only when needed.

    if (((MFX_MEMTYPE_FROM_VPPIN & request->Type)
            && (DXGI_FORMAT_YUY2 == desc.Format))
        || (DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format)
        || (DXGI_FORMAT_R10G10B10A2_UNORM == desc.Format)) {
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;
      if (desc.ArraySize > 2)
        goto error;
    }

    if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type)
        || (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type)) {
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;
      if (desc.ArraySize > 2)
        goto error;
    }

    if (DXGI_FORMAT_P8 == desc.Format)
      desc.BindFlags = 0;

    /* Create surface textures */
    for (i = 0; i < response_data->num_surfaces / desc.ArraySize; i++) {
      hr = ID3D11Device_CreateTexture2D ((ID3D11Device *) d3d11_device, &desc,
          NULL, &texture);
      if (FAILED (hr))
        goto error;

      response_data->mids[i]->mid = texture;
      response_data->mids[i]->info = &response_data->frame_info;
    }

    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;        // | D3D11_CPU_ACCESS_WRITE;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    //desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    /* Create surface staging textures */
    for (i = 0; i < response_data->num_surfaces; i++) {
      hr = ID3D11Device_CreateTexture2D ((ID3D11Device *) d3d11_device, &desc,
          NULL, &texture);
      if (FAILED (hr))
        goto error;

      response_data->mids[i]->mid_stage = texture;
    }
  }

  response->mids = (mfxMemId *) response_data->mids;
  response->NumFrameActual = response_data->num_surfaces;

  response_data->response = *response;
  priv->saved_responses = g_list_prepend (priv->saved_responses, response_data);

  return MFX_ERR_NONE;

error:
  free_memory_ids (response_data);
  g_free (response_data);
  return MFX_ERR_MEMORY_ALLOC;
}

mfxStatus
gst_mfx_task_frame_free (mfxHDL pthis, mfxFrameAllocResponse * response)
{
  GstMfxTask *task =
      gst_mfx_task_aggregator_get_current_task (GST_MFX_TASK_AGGREGATOR
      (pthis));
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE (task);
  ResponseData *response_data;

  GList *l = g_list_find_custom (priv->saved_responses, response,
      find_response);
  if (!l)
    return MFX_ERR_NOT_FOUND;

  response_data = l->data;

  free_memory_ids (response_data);

  priv->saved_responses = g_list_delete_link (priv->saved_responses, l);
  g_free (response_data);

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_lock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxContext *context =
      gst_mfx_task_aggregator_get_context (GST_MFX_TASK_AGGREGATOR (pthis));
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;
  HRESULT hr = S_OK;
  D3D11_RESOURCE_DIMENSION resource_type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  D3D11_MAPPED_SUBRESOURCE locked_rect = { 0 };
  ID3D11Resource *d3d11_resource = (ID3D11Resource *) mem_id->mid;
  ID3D11DeviceContext *d3d11_context =
      gst_mfx_d3d11_device_get_d3d11_context (gst_mfx_context_get_device
      (context));

  gst_mfx_context_unref (context);

  ID3D11Resource_GetType (d3d11_resource, &resource_type);

  if (resource_type == D3D11_RESOURCE_DIMENSION_BUFFER) {
    hr = ID3D11DeviceContext_Map (d3d11_context, d3d11_resource, 0,
        D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &locked_rect);
    if (FAILED (hr))
      return MFX_ERR_LOCK_MEMORY;

    ptr->Pitch = (mfxU16) locked_rect.RowPitch;
    ptr->Y = (mfxU8 *) locked_rect.pData;
    ptr->U = 0;
    ptr->V = 0;
  } else {
    D3D11_TEXTURE2D_DESC desc = { 0 };
    ID3D11Texture2D_GetDesc ((ID3D11Texture2D *) d3d11_resource, &desc);
    ID3D11Texture2D *stage = (ID3D11Texture2D *) mem_id->mid_stage;
    g_return_val_if_fail (stage != NULL, MFX_ERR_INVALID_HANDLE);
    /* copy data only when reading from stored surface */
    if (mem_id->rw & MFX_SURFACE_READ)
      ID3D11DeviceContext_CopyResource (d3d11_context,
          (ID3D11Resource *) stage, (ID3D11Resource *) d3d11_resource);
    /*ID3D11Resource is a parent of ID3D11Texture2D, casting is safe */

    do {
      hr = ID3D11DeviceContext_Map (d3d11_context, (ID3D11Resource *) stage,
          0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &locked_rect);
      if (S_OK != hr && DXGI_ERROR_WAS_STILL_DRAWING != hr)
        return MFX_ERR_LOCK_MEMORY;
    } while (DXGI_ERROR_WAS_STILL_DRAWING == hr);

    if (FAILED (hr))
      return MFX_ERR_LOCK_MEMORY;

    switch (desc.Format) {
      case DXGI_FORMAT_NV12:
      case DXGI_FORMAT_P010:
        ptr->Pitch = (mfxU16) locked_rect.RowPitch;
        ptr->Y = (mfxU8 *) locked_rect.pData;
        ptr->U = (mfxU8 *) locked_rect.pData
            + desc.Height * locked_rect.RowPitch;
        ptr->V = (desc.Format == DXGI_FORMAT_P010) ? ptr->U + 2 : ptr->U + 1;
        break;
      case DXGI_FORMAT_420_OPAQUE:
        ptr->Pitch = (mfxU16) locked_rect.RowPitch;
        ptr->Y = (mfxU8 *) locked_rect.pData;
        ptr->V = ptr->Y + desc.Height * locked_rect.RowPitch;
        ptr->U = ptr->V + (desc.Height * locked_rect.RowPitch) / 4;
        break;
      case DXGI_FORMAT_B8G8R8A8_UNORM:
        ptr->Pitch = (mfxU16) locked_rect.RowPitch;
        ptr->B = (mfxU8 *) locked_rect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
      case DXGI_FORMAT_YUY2:
        ptr->Pitch = (mfxU16) locked_rect.RowPitch;
        ptr->Y = (mfxU8 *) locked_rect.pData;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;
      default:
        return MFX_ERR_UNSUPPORTED;
    }
  }
  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_unlock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxContext *context =
      gst_mfx_task_aggregator_get_context (GST_MFX_TASK_AGGREGATOR (pthis));
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;
  D3D11_RESOURCE_DIMENSION resource_type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  ID3D11Resource *d3d11_resource = (ID3D11Resource *) mem_id->mid;
  ID3D11DeviceContext *d3d11_context =
      gst_mfx_d3d11_device_get_d3d11_context (gst_mfx_context_get_device
      (context));

  gst_mfx_context_unref (context);

  ID3D11Resource_GetType (d3d11_resource, &resource_type);

  if (resource_type == D3D11_RESOURCE_DIMENSION_BUFFER) {
    ID3D11DeviceContext_Unmap (d3d11_context, (ID3D11Resource *) d3d11_resource,
        0);
  } else {
    ID3D11Texture2D *stage = (ID3D11Texture2D *) mem_id->mid_stage;

    ID3D11DeviceContext_Unmap (d3d11_context, (ID3D11Resource *) stage, 0);
    /* copy data only when writing to stored surface */
    if (mem_id->rw & MFX_SURFACE_WRITE)
      ID3D11DeviceContext_CopyResource (d3d11_context,
          (ID3D11Resource *) d3d11_resource, (ID3D11Resource *) stage);
  }

  if (ptr) {
    ptr->Pitch = 0;
    ptr->U = ptr->V = ptr->Y = 0;
    ptr->A = ptr->R = ptr->G = ptr->B = 0;
  }
  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_get_hdl (mfxHDL pthis, mfxMemId mid, mfxHDL * hdl)
{
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;

  if (!mem_id || !mem_id->mid || !hdl)
    return MFX_ERR_INVALID_HANDLE;

  mfxHDLPair *pair = (mfxHDLPair *) hdl;
  pair->first = mem_id->mid;
  pair->second = 0;

  return MFX_ERR_NONE;
}
