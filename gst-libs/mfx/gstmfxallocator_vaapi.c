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

  return _response_data ? _response->mids != _response_data->mids : -1;
}

static void
free_mids (ResponseData * response)
{
  g_slice_free1 (response->num_surfaces * sizeof (VABufferID),
      response->coded_buf);
  g_slice_free1 (response->num_surfaces * sizeof (GstMfxMemoryId),
      response->mem_ids);
  g_slice_free1 (response->num_surfaces * sizeof (mfxMemId), response->mids);
  g_slice_free1 (response->num_surfaces * sizeof (VASurfaceID),
      response->surfaces);
}

mfxStatus
gst_mfx_task_frame_alloc (mfxHDL pthis, mfxFrameAllocRequest * request,
    mfxFrameAllocResponse * response)
{
  GstMfxTask *task =
      gst_mfx_task_aggregator_get_current_task (GST_MFX_TASK_AGGREGATOR
      (pthis));
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE (task);
  GstMfxDisplay *const display = gst_mfx_context_get_device (priv->context);
  mfxFrameInfo *info;
  mfxStatus mfx_sts;
  VASurfaceAttrib attrib;
  VAStatus va_sts;
  guint fourcc, i, range;
  GstMfxMemoryId *mid;
  mfxU16 num_surfaces;
  mfxU16 num_surfaces_range[2];
  ResponseData *response_data;

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

  if (!(request->Type
      & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET
          | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))) {
    GST_ERROR ("Unsupported surface type: %d\n", request->Type);
    return MFX_ERR_UNSUPPORTED;
  }

  response_data = g_malloc0 (sizeof (ResponseData));
  response_data->frame_info = request->Info;
  info = &response_data->frame_info;

  if (request->Type & MFX_MEMTYPE_INTERNAL_FRAME) {
    num_surfaces_range[0] = request->NumFrameSuggested;
    num_surfaces_range[1] = request->NumFrameMin;
  }
  else {
    num_surfaces_range[0] = priv->request.NumFrameSuggested;
    num_surfaces_range[1] = priv->request.NumFrameMin;
  }

  for (range = 0; range < 2; range++) {
    num_surfaces = response_data->num_surfaces = num_surfaces_range[range];
    response_data->mem_ids =
        g_slice_alloc (num_surfaces * sizeof (GstMfxMemoryId));
    response_data->mids = g_slice_alloc (num_surfaces * sizeof (mfxMemId));

    if (!response_data->mem_ids || !response_data->mids)
      goto cleanup;

    if (info->FourCC != MFX_FOURCC_P8) {
      response_data->surfaces =
          g_slice_alloc0 (num_surfaces * sizeof (VASurfaceID));
      if (!response_data->surfaces)
        goto cleanup;

      fourcc = gst_mfx_video_format_to_va_fourcc (info->FourCC);
      attrib.type = VASurfaceAttribPixelFormat;
      attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
      attrib.value.type = VAGenericValueTypeInteger;
      attrib.value.value.i = fourcc;

      GST_MFX_DISPLAY_LOCK (display);
      va_sts = vaCreateSurfaces (GST_MFX_DISPLAY_VADISPLAY (display),
          gst_mfx_video_format_to_va_format (info->FourCC),
          request->Info.Width, request->Info.Height,
          response_data->surfaces, num_surfaces, &attrib, 1);
      GST_MFX_DISPLAY_UNLOCK (display);
      if (!vaapi_check_status (va_sts, "vaCreateSurfaces ()")) {
        GST_ERROR ("Error allocating VA surfaces %d", va_sts);
        goto cleanup;
      }

      for (i = 0; i < num_surfaces; i++) {
        mid = &response_data->mem_ids[i];
        mid->mid = &response_data->surfaces[i];
        mid->info = &response_data->frame_info;

        response_data->mids[i] = mid;
      }
    } else {
      VAContextID context_id = request->reserved[0];
      int width32 = 32 * ((request->Info.Width + 31) >> 5);
      int height32 = 32 * ((request->Info.Height + 31) >> 5);
      int codedbuf_size = (width32 * height32) * 400LL / (16 * 16);

      response_data->coded_buf =
          g_slice_alloc (num_surfaces * sizeof (VABufferID));
      if (!response_data->coded_buf)
        goto cleanup;

      for (i = 0; i < num_surfaces; i++) {
        va_sts = vaCreateBuffer (GST_MFX_DISPLAY_VADISPLAY (display),
            context_id, VAEncCodedBufferType, codedbuf_size,
            1, NULL, &response_data->coded_buf[i]);
        if (!vaapi_check_status (va_sts, "vaCreateBuffer ()")) {
          GST_ERROR ("Error allocating VA buffers %d", va_sts);
          goto cleanup;
        }
        mid = &response_data->mem_ids[i];
        mid->mid = &response_data->coded_buf[i];
        mid->info = &response_data->frame_info;

        response_data->mids[i] = mid;
      }
    }
    mfx_sts = MFX_ERR_NONE;
    break;
cleanup:
    free_mids (response_data);
    mfx_sts = MFX_ERR_MEMORY_ALLOC;
  }

  if (MFX_ERR_NONE == mfx_sts) {
    response->mids = response_data->mids;
    response->NumFrameActual = num_surfaces;

    response_data->response = *response;
    priv->saved_responses =
        g_list_prepend (priv->saved_responses, response_data);
  }
  else {
    GST_ERROR ("Error allocating MFX surfaces %d", mfx_sts);
  }

  return mfx_sts;
}

mfxStatus
gst_mfx_task_frame_free (mfxHDL pthis, mfxFrameAllocResponse * response)
{
  GstMfxTask *task =
      gst_mfx_task_aggregator_get_current_task (GST_MFX_TASK_AGGREGATOR
      (pthis));
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE (task);
  GstMfxDisplay *const display = gst_mfx_context_get_device (priv->context);
  mfxFrameInfo *info;
  mfxU16 i, num_surfaces;
  ResponseData *response_data;

  GList *l = g_list_find_custom (priv->saved_responses, response,
      find_response);
  if (!l)
    return MFX_ERR_NOT_FOUND;

  response_data = l->data;
  info = &response_data->frame_info;

  num_surfaces = response_data->num_surfaces;

  if (info->FourCC != MFX_FOURCC_P8) {
    GST_MFX_DISPLAY_LOCK (display);
    vaDestroySurfaces (GST_MFX_DISPLAY_VADISPLAY (display),
        response_data->surfaces, num_surfaces);
    GST_MFX_DISPLAY_UNLOCK (display);
  } else {
    for (i = 0; i < num_surfaces; i++) {
      GST_MFX_DISPLAY_LOCK (display);
      vaDestroyBuffer (GST_MFX_DISPLAY_VADISPLAY (display),
          response_data->coded_buf[i]);
      GST_MFX_DISPLAY_UNLOCK (display);
    }
  }
  free_mids (response_data);
  priv->saved_responses = g_list_delete_link (priv->saved_responses, l);
  g_free (response_data);

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_lock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxTask *task =
      gst_mfx_task_aggregator_get_current_task (GST_MFX_TASK_AGGREGATOR
      (pthis));
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE (task);
  GstMfxDisplay *const display = gst_mfx_context_get_device (priv->context);
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;
  VAStatus sts;

  if (mem_id->info->FourCC == MFX_FOURCC_P8) {
    VACodedBufferSegment *coded_buffer_segment;

    GST_MFX_DISPLAY_LOCK (display);
    sts = vaMapBuffer (GST_MFX_DISPLAY_VADISPLAY (display),
        *(VABufferID *) mem_id->mid, (void **) &coded_buffer_segment);
    GST_MFX_DISPLAY_UNLOCK (display);
    if (!vaapi_check_status (sts, "vaMapBuffer ()")) {
      GST_ERROR ("Error mapping VA buffers %d", sts);
      return MFX_ERR_LOCK_MEMORY;
    }
    ptr->Y = (mfxU8 *) coded_buffer_segment->buf;
  } else
    return MFX_ERR_UNSUPPORTED;

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_unlock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxTask *task =
      gst_mfx_task_aggregator_get_current_task (GST_MFX_TASK_AGGREGATOR
      (pthis));
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE (task);
  GstMfxDisplay *const display = gst_mfx_context_get_device (priv->context);
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;

  if (mem_id->info->FourCC == MFX_FOURCC_P8) {
    GST_MFX_DISPLAY_LOCK (display);
    vaUnmapBuffer (GST_MFX_DISPLAY_VADISPLAY (display),
        *(VABufferID *) mem_id->mid);
    GST_MFX_DISPLAY_UNLOCK (display);
  } else
    return MFX_ERR_UNSUPPORTED;

  return MFX_ERR_NONE;
}

mfxStatus
gst_mfx_task_frame_get_hdl (mfxHDL pthis, mfxMemId mid, mfxHDL * hdl)
{
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;

  if (!mem_id || !mem_id->mid || !hdl)
    return MFX_ERR_INVALID_HANDLE;

  *hdl = mem_id->mid;
  return MFX_ERR_NONE;
}
