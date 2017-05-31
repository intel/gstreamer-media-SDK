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

#include "gstmfxallocator_d3d11.h"
#include "gstmfxtask.h"
#include "gstmfxtask_priv.h"
#include "gstmfxcontext.h"
#include "d3d11/gstmfxdevice.h"

static gint
find_response(gconstpointer response_data, gconstpointer response)
{
  ResponseData *_response_data = (ResponseData *)response_data;
  mfxFrameAllocResponse *_response = (mfxFrameAllocResponse *)response;

  return _response_data ? _response->mids != _response_data->mids : -1;
}

mfxStatus
gst_mfx_task_frame_alloc(mfxHDL pthis, mfxFrameAllocRequest * request,
  mfxFrameAllocResponse * response)
{
  GstMfxTask *task = pthis;
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  ID3D11Device *d3d11_device =
    gst_mfx_device_get_handle(gst_mfx_context_get_device(priv->context));
  HRESULT hr = S_OK;
  ResponseData *response_data;

  /*if (priv->task_type & (GST_MFX_TASK_VPP_IN | GST_MFX_TASK_ENCODER))
    request->Type |= 0x2000;*/
  if (priv->task_type & (GST_MFX_TASK_VPP_OUT | GST_MFX_TASK_DECODER))
    request->Type |= 0x1000;

  if (priv->saved_responses && (priv->task_type & GST_MFX_TASK_DECODER)) {
    GList *l = g_list_last(priv->saved_responses);
    if (l) {
      response_data = l->data;
      *response = *response_data->response;
      return MFX_ERR_NONE;
    }
  }

  memset(response, 0, sizeof(mfxFrameAllocResponse));

  response_data = g_malloc0(sizeof(ResponseData));
  response_data->frame_info = request->Info;
  response_data->num_surfaces = request->NumFrameSuggested;

  /* Allocate custom container to keep texture and stage buffers for each surface.
  * Container also stores the intended read and/or write operation. */
  response_data->mids =
    g_slice_alloc0(response_data->num_surfaces * sizeof(GstMfxMemoryId *));
  if (!response_data->mids)
    return MFX_ERR_MEMORY_ALLOC;

  for (int i = 0; i < response_data->num_surfaces; i++) {
    response_data->mids[i] = g_slice_new0(GstMfxMemoryId);
    if (!response_data->mids[i])
      return MFX_ERR_MEMORY_ALLOC;
    response_data->mids[i]->rw = request->Type & 0xF000; // Set intended read/write operation
  }

  request->Type = request->Type & 0x0FFF;

  if (MFX_FOURCC_P8 == request->Info.FourCC) {
    D3D11_BUFFER_DESC desc = { 0 };

    if (!request->NumFrameSuggested)
      return MFX_ERR_MEMORY_ALLOC;

    desc.ByteWidth = request->Info.Width * request->Info.Height;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    ID3D11Buffer* buffer = 0;
    hr = ID3D11Device_CreateBuffer((ID3D11Device*)d3d11_device, &desc, 0, &buffer);
    if (FAILED(hr))
      return MFX_ERR_MEMORY_ALLOC;

    response_data->mids[0]->mid = (ID3D11Texture2D*)(buffer);
    response_data->mids[0]->info = &response_data->frame_info;
  }
  else {
    ID3D11Texture2D *texture;
    DXGI_FORMAT format;
    D3D11_TEXTURE2D_DESC desc = { 0 };

    format = gst_mfx_fourcc_to_dxgi_format(request->Info.FourCC);
    if (DXGI_FORMAT_UNKNOWN == format)
      return MFX_ERR_UNSUPPORTED;

    desc.Width = request->Info.Width;
    desc.Height = request->Info.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1; // number of subresources is 1 in this case
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.MiscFlags = 0;
    //desc.MiscFlags            = D3D11_RESOURCE_MISC_SHARED;

    if ((MFX_MEMTYPE_FROM_VPPIN & request->Type)
        && (DXGI_FORMAT_YUY2 == desc.Format)
        || (DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format)) {
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;
      if (desc.ArraySize > 2)
        return MFX_ERR_MEMORY_ALLOC;
    }

    if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type)
      || (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type)) {
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;
      if (desc.ArraySize > 2)
        return MFX_ERR_MEMORY_ALLOC;
    }

#if MSDK_CHECK_VERSION(1,19)
    if (request->Type & MFX_MEMTYPE_SHARED_RESOURCE)
    {
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
      desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    }
#endif

    if (DXGI_FORMAT_P8 == desc.Format)
      desc.BindFlags = 0;

    /* Create surface textures */
    for (size_t i = 0; i < request->NumFrameSuggested / desc.ArraySize; i++) {
      hr =
        ID3D11Device_CreateTexture2D((ID3D11Device*)d3d11_device, &desc,
          NULL, &texture);
      if (FAILED(hr))
        return MFX_ERR_MEMORY_ALLOC;

      response_data->mids[i]->mid = texture;
    }

    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    //desc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED;

    /* Create surface staging textures */
    for (size_t i = 0; i < request->NumFrameSuggested; i++) {
      hr =
        ID3D11Device_CreateTexture2D((ID3D11Device*)d3d11_device, &desc,
          NULL, &texture);
      if (FAILED(hr))
        return MFX_ERR_MEMORY_ALLOC;

      response_data->mids[i]->mid_stage = texture;
    }
  }

  response->mids = (mfxMemId *)response_data->mids;
  response->NumFrameActual = request->NumFrameSuggested;

  response_data->response = response;
  priv->saved_responses = g_list_prepend(priv->saved_responses, response_data);

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_free(mfxHDL pthis, mfxFrameAllocResponse * response)
{
  GstMfxTask *task = pthis;
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  mfxU16 i, num_surfaces;
  ResponseData *response_data;

  GList *l = g_list_find_custom(priv->saved_responses, response,
    find_response);
  if (!l)
    return MFX_ERR_NOT_FOUND;

  response_data = l->data;
  num_surfaces = response_data->num_surfaces;

  if (response_data->mids) {
    for (i = 0; i < num_surfaces; i++) {
      if (response_data->mids[i]) {
        GstMfxMemoryId *mem_id = (GstMfxMemoryId *)response_data->mids[i];
        ID3D11Texture2D *surface = (ID3D11Texture2D*)mem_id->mid;
        ID3D11Texture2D *stage = (ID3D11Texture2D*)mem_id->mid_stage;

        if (surface)
          ID3D11Texture2D_Release(surface);
        if (stage)
          ID3D11Texture2D_Release(stage);

        g_slice_free(GstMfxMemoryId, mem_id);
      }
    }
    g_slice_free1(num_surfaces * sizeof(GstMfxMemoryId*), response_data->mids);
    response_data->mids = NULL;
  }

  priv->saved_responses = g_list_delete_link(priv->saved_responses, l);
  g_free(response_data);

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxTask *task = pthis;
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *)mid;

  if (mem_id->info && mem_id->info->FourCC == MFX_FOURCC_P8) {
    D3D11_MAPPED_SUBRESOURCE locked_rect = { 0 };
    ID3D11Texture2D *texture = (ID3D11Texture2D *)mem_id->mid;
    ID3D11DeviceContext *d3d11_context =
      gst_mfx_device_get_d3d11_context(gst_mfx_context_get_device(priv->context));

    HRESULT hr = ID3D11DeviceContext_Map(d3d11_context,
      texture, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &locked_rect);
    if (FAILED(hr))
      return MFX_ERR_LOCK_MEMORY;

    ptr->Pitch = (mfxU16)locked_rect.RowPitch;
    ptr->Y = (mfxU8*)locked_rect.pData;
    ptr->U = 0;
    ptr->V = 0;
  }
  else {
    return MFX_ERR_UNSUPPORTED;
  }

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxTask *task = pthis;
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *)mid;

  if (mem_id->info->FourCC == MFX_FOURCC_P8) {
    ID3D11Texture2D *texture = (ID3D11Texture2D *)mem_id->mid;
    ID3D11DeviceContext *d3d11_context =
      gst_mfx_device_get_d3d11_context(gst_mfx_context_get_device(priv->context));
    ID3D11DeviceContext_Unmap(d3d11_context, texture, 0);
  }
  else {
    return MFX_ERR_UNSUPPORTED;
  }

  if (ptr) {
    ptr->Pitch = 0;
    ptr->U = ptr->V = ptr->Y = 0;
    ptr->A = ptr->R = ptr->G = ptr->B = 0;
  }
  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_get_hdl(mfxHDL pthis, mfxMemId mid, mfxHDL * hdl)
{
  pthis;
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *)mid;

  if (!mem_id || !mem_id->mid || !hdl)
    return MFX_ERR_INVALID_HANDLE;

  mfxHDLPair *pair = (mfxHDLPair *)hdl;
  pair->first = mem_id->mid;
  pair->second = 0;

  return MFX_ERR_NONE;
}
