/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
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

#include "gstmfxtask.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxutils_vaapi.h"
#include "video-format.h"
#include "gstmfxtypes.h"

#define DEBUG 1
#include "gstmfxdebug.h"

typedef struct _ResponseData ResponseData;
struct _ResponseData
{
  GstMfxMemoryId *mem_ids;
  mfxMemId *mids;
  VASurfaceID *surfaces;
  VABufferID *coded_buf;
  mfxU16 num_surfaces;
  mfxFrameAllocResponse *response;
  mfxFrameInfo frame_info;
  guint num_used;
};

struct _GstMfxTask
{
  GstMfxMiniObject parent_instance;

  GstMfxDisplay *display;
  GstMfxTaskAggregator *aggregator;
  GList *saved_responses;
  mfxFrameAllocRequest request;
  mfxVideoParam params;
  mfxSession session;
  guint task_type;
  gboolean memtype_is_system;
  gboolean is_joined;

  /* This variable use to handle re-use back VASurfaces */
  gboolean soft_reinit;
  mfxU16 backup_num_surfaces;
  VASurfaceID *backup_surfaces;
};

static gint
find_response (gconstpointer response_data, gconstpointer response)
{
  ResponseData *_response_data = (ResponseData *) response_data;
  mfxFrameAllocResponse *_response = (mfxFrameAllocResponse *) response;

  return _response_data ? _response->mids != _response_data->mids : -1;
}

mfxStatus
gst_mfx_task_frame_alloc (mfxHDL pthis, mfxFrameAllocRequest * req,
    mfxFrameAllocResponse * resp)
{
  GstMfxTask *task = pthis;
  mfxFrameInfo *info;
  VASurfaceAttrib attrib;
  VAStatus sts;
  guint fourcc, i;
  GstMfxMemoryId *mid;
  mfxU16 num_surfaces;
  ResponseData *response_data;

  if (task->saved_responses && task->task_type & GST_MFX_TASK_DECODER) {
    GList *l = g_list_last (task->saved_responses);
    if (l) {
      response_data = l->data;
      *resp = *response_data->response;
      return MFX_ERR_NONE;
    }
  }

  memset (resp, 0, sizeof (mfxFrameAllocResponse));

  if (!(req->Type
      & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET
        | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))) {
    GST_ERROR ("Unsupported surface type: %d\n", req->Type);
    return MFX_ERR_UNSUPPORTED;
  }

  response_data = g_malloc0 (sizeof (ResponseData));
  response_data->frame_info = req->Info;
  info = &response_data->frame_info;

  if (info->FourCC != MFX_FOURCC_P8) {
    response_data->num_surfaces =
        task->request.NumFrameSuggested < req->NumFrameSuggested ?
        req->NumFrameSuggested : task->request.NumFrameSuggested;
  }
  else {
    response_data->num_surfaces = req->NumFrameSuggested;
  }

  if (task->soft_reinit && (response_data->num_surfaces != task->backup_num_surfaces)
      && (info->FourCC != MFX_FOURCC_P8))
    response_data->num_surfaces = task->backup_num_surfaces;
  else if ((task->task_type & GST_MFX_TASK_DECODER) &&
           info->Width < 1281 && info->Height < 721 &&
           info->FrameRateExtN > 50)
    response_data->num_surfaces += 5;

  num_surfaces = response_data->num_surfaces;

  response_data->mem_ids =
      g_slice_alloc (num_surfaces * sizeof (GstMfxMemoryId));
  response_data->mids = g_slice_alloc (num_surfaces * sizeof (mfxMemId));

  if (!response_data->mem_ids || !response_data->mids)
    goto error_allocate_memory;

  if (info->FourCC != MFX_FOURCC_P8) {
    if (task->soft_reinit) {
      if (task->backup_surfaces == NULL) {
	GST_ERROR ("Failed reuse back VA surfaces");
	goto error_allocate_memory;
      }
      response_data->surfaces = task->backup_surfaces;
      task->soft_reinit = FALSE;
      task->backup_num_surfaces = 0;
      task->backup_surfaces = NULL;
    } else {
      response_data->surfaces =
          g_slice_alloc0 (num_surfaces * sizeof (VASurfaceID));

      if (!response_data->surfaces)
        goto error_allocate_memory;

      fourcc = gst_mfx_video_format_to_va_fourcc (info->FourCC);
      attrib.type = VASurfaceAttribPixelFormat;
      attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
      attrib.value.type = VAGenericValueTypeInteger;
      attrib.value.value.i = fourcc;

      GST_MFX_DISPLAY_LOCK (task->display);
      sts = vaCreateSurfaces (GST_MFX_DISPLAY_VADISPLAY (task->display),
          gst_mfx_video_format_to_va_format (info->FourCC),
          req->Info.Width, req->Info.Height,
          response_data->surfaces, num_surfaces, &attrib, 1);
      GST_MFX_DISPLAY_UNLOCK (task->display);
      if (!vaapi_check_status (sts, "vaCreateSurfaces ()")) {
        GST_ERROR ("Error allocating VA surfaces %d", sts);
        goto error_allocate_memory;
      }
    }
    for (i = 0; i < num_surfaces; i++) {
      mid = &response_data->mem_ids[i];
      mid->mid = &response_data->surfaces[i];
      mid->info = &response_data->frame_info;

      response_data->mids[i] = mid;
    }
  } else {
    VAContextID context_id = req->reserved[0];
    int width32 =  32 * ((req->Info.Width + 31) >> 5);
    int height32 = 32 * ((req->Info.Height + 31) >> 5);
    int codedbuf_size = (width32 * height32) * 400LL / (16 * 16);

    response_data->coded_buf =
        g_slice_alloc (num_surfaces * sizeof (VABufferID));

    for (i = 0; i < num_surfaces; i++) {
      sts = vaCreateBuffer (GST_MFX_DISPLAY_VADISPLAY (task->display),
          context_id, VAEncCodedBufferType, codedbuf_size,
          1, NULL, &response_data->coded_buf[i]);
      if (!vaapi_check_status (sts, "vaCreateBuffer ()")) {
        GST_ERROR ("Error allocating VA buffers %d", sts);
        goto error_allocate_memory;
      }
      mid = &response_data->mem_ids[i];
      mid->mid = &response_data->coded_buf[i];
      mid->info = &response_data->frame_info;

      response_data->mids[i] = mid;
    }
  }

  resp->mids = response_data->mids;
  resp->NumFrameActual = num_surfaces;

  response_data->response = resp;
  task->saved_responses = g_list_prepend (task->saved_responses, response_data);

  return MFX_ERR_NONE;

error_allocate_memory:
  {
    if (response_data->coded_buf)
      g_slice_free1 (num_surfaces * sizeof (VABufferID),
          response_data->coded_buf);

    if (response_data->mem_ids)
      g_slice_free1 (num_surfaces * sizeof (GstMfxMemoryId),
          response_data->mem_ids);

    if (response_data->mids)
      g_slice_free1 (num_surfaces * sizeof (mfxMemId), response_data->mids);

    if (response_data->surfaces)
      g_slice_free1 (num_surfaces * sizeof (VASurfaceID),
          response_data->surfaces);

    return MFX_ERR_MEMORY_ALLOC;
  }
}

mfxStatus
gst_mfx_task_frame_free (mfxHDL pthis, mfxFrameAllocResponse * resp)
{
  GstMfxTask *task = pthis;
  mfxFrameInfo *info;
  mfxU16 i, num_surfaces;
  ResponseData *response_data;

  GList *l = g_list_find_custom (task->saved_responses, resp,
      find_response);
  if (!l)
    return MFX_ERR_NOT_FOUND;

  response_data = l->data;
  info = &response_data->frame_info;

  num_surfaces = response_data->num_surfaces;

  if (info->FourCC != MFX_FOURCC_P8) {
    if (task->soft_reinit) {
      task->backup_num_surfaces = num_surfaces;
      task->backup_surfaces = response_data->surfaces;
    } else {
      GST_MFX_DISPLAY_LOCK (task->display);
      vaDestroySurfaces (GST_MFX_DISPLAY_VADISPLAY (task->display),
          response_data->surfaces, num_surfaces);
      GST_MFX_DISPLAY_UNLOCK (task->display);

      g_slice_free1 (num_surfaces * sizeof (VASurfaceID),
          response_data->surfaces);
    }
  } else {
    for (i = 0; i < num_surfaces; i++) {
      GST_MFX_DISPLAY_LOCK (task->display);
      vaDestroyBuffer (GST_MFX_DISPLAY_VADISPLAY (task->display),
          response_data->coded_buf[i]);
      GST_MFX_DISPLAY_UNLOCK (task->display);
    }
    g_slice_free1 (num_surfaces * sizeof (VABufferID),
        response_data->coded_buf);
  }

  g_slice_free1 (num_surfaces * sizeof (GstMfxMemoryId),
      response_data->mem_ids);
  g_slice_free1 (num_surfaces * sizeof (mfxMemId), response_data->mids);

  task->saved_responses = g_list_delete_link (task->saved_responses, l);
  g_free (response_data);

  return MFX_ERR_NONE;
}

static mfxStatus
gst_mfx_task_frame_lock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxTask *task = pthis;
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;
  VAStatus sts;

  if (mem_id->info->FourCC == MFX_FOURCC_P8) {
    VACodedBufferSegment *coded_buffer_segment;

    GST_MFX_DISPLAY_LOCK (task->display);
    sts = vaMapBuffer (GST_MFX_DISPLAY_VADISPLAY (task->display),
        *(VABufferID *) mem_id->mid, (void **) &coded_buffer_segment);
    GST_MFX_DISPLAY_UNLOCK (task->display);
    if (!vaapi_check_status (sts, "vaMapBuffer ()")) {
      GST_ERROR ("Error mapping VA buffers %d", sts);
      return MFX_ERR_LOCK_MEMORY;
    }
    ptr->Y = (mfxU8 *) coded_buffer_segment->buf;
  } else
    return MFX_ERR_UNSUPPORTED;

  return MFX_ERR_NONE;
}

static mfxStatus
gst_mfx_task_frame_unlock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstMfxTask *task = pthis;
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;

  if (mem_id->info->FourCC == MFX_FOURCC_P8) {
    GST_MFX_DISPLAY_LOCK (task->display);
    vaUnmapBuffer (GST_MFX_DISPLAY_VADISPLAY (task->display),
        *(VABufferID *) mem_id->mid);
    GST_MFX_DISPLAY_UNLOCK (task->display);
  } else
    return MFX_ERR_UNSUPPORTED;

  return MFX_ERR_NONE;
}

static mfxStatus
gst_mfx_task_frame_get_hdl (mfxHDL pthis, mfxMemId mid, mfxHDL * hdl)
{
  GstMfxMemoryId *mem_id = (GstMfxMemoryId *) mid;

  if (!mem_id || !mem_id->mid || !hdl)
    return MFX_ERR_INVALID_HANDLE;

  *hdl = mem_id->mid;
  return MFX_ERR_NONE;
}

GstMfxDisplay *
gst_mfx_task_get_display (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, 0);

  return gst_mfx_display_ref(task->display);
}

mfxSession
gst_mfx_task_get_session (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, 0);

  return task->session;
}

GstMfxMemoryId *
gst_mfx_task_get_memory_id (GstMfxTask * task)
{
  GList *l;
  ResponseData *response_data;

  g_return_val_if_fail (task != NULL, 0);

  l = g_list_first (task->saved_responses);
  response_data = l->data;

  return &response_data->mem_ids[response_data->num_used++];
}

guint
gst_mfx_task_get_num_surfaces (GstMfxTask * task)
{
  GList *l;
  ResponseData *response_data;

  g_return_val_if_fail (task != NULL, 0);

  l = g_list_first (task->saved_responses);
  response_data = l->data;

  return response_data->num_surfaces;
}

mfxFrameAllocRequest *
gst_mfx_task_get_request (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, NULL);

  return &task->request;
}

void
gst_mfx_task_set_request (GstMfxTask * task, mfxFrameAllocRequest * request)
{
  g_return_if_fail (task != NULL);

  task->request = *request;
}

gboolean
gst_mfx_task_has_type (GstMfxTask * task, guint flags)
{
  g_return_val_if_fail (task != NULL, FALSE);

  return (task->task_type & flags);
}

void
gst_mfx_task_set_task_type (GstMfxTask * task, guint flags)
{
  g_return_if_fail (task != NULL);

  task->task_type = flags;
}

guint
gst_mfx_task_get_task_type (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, GST_MFX_TASK_INVALID);

  return task->task_type;
}

void
gst_mfx_task_ensure_memtype_is_system (GstMfxTask * task)
{
  g_return_if_fail (task != NULL);

  task->memtype_is_system = TRUE;
}

void
gst_mfx_task_use_video_memory (GstMfxTask * task)
{
  mfxFrameAllocator frame_allocator = {
    .pthis = task,
    .Alloc = gst_mfx_task_frame_alloc,
    .Lock = gst_mfx_task_frame_lock,
    .Unlock = gst_mfx_task_frame_unlock,
    .Free = gst_mfx_task_frame_free,
    .GetHDL = gst_mfx_task_frame_get_hdl,
  };

  MFXVideoCORE_SetFrameAllocator (task->session, &frame_allocator);
  task->memtype_is_system = FALSE;
}

gboolean
gst_mfx_task_has_video_memory (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, FALSE);

  return !task->memtype_is_system;
}

void
gst_mfx_task_set_video_params (GstMfxTask * task, mfxVideoParam * params)
{
  g_return_if_fail (task != NULL);

  task->params = *params;
}

mfxVideoParam *
gst_mfx_task_get_video_params (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, NULL);

  return &task->params;
}

void
gst_mfx_task_update_video_params (GstMfxTask * task, mfxVideoParam * params)
{
  params->AsyncDepth = task->params.AsyncDepth;
  params->IOPattern = task->params.IOPattern;
}

static void
gst_mfx_task_finalize (GstMfxTask * task)
{
  if (task->is_joined) {
    MFXDisjoinSession (task->session);
    MFXClose (task->session);
  }
  gst_mfx_task_aggregator_remove_task (task->aggregator, task);
  gst_mfx_task_aggregator_unref (task->aggregator);
  gst_mfx_display_unref (task->display);
  g_list_free_full (task->saved_responses, g_free);
}


static inline const GstMfxMiniObjectClass *
gst_mfx_task_class (void)
{
  static const GstMfxMiniObjectClass GstMfxTaskClass = {
    sizeof (GstMfxTask),
    (GDestroyNotify) gst_mfx_task_finalize
  };
  return &GstMfxTaskClass;
}

static void
gst_mfx_task_init (GstMfxTask * task, GstMfxTaskAggregator * aggregator,
    mfxSession session, guint type_flags, gboolean is_joined)
{
  task->is_joined = is_joined;
  task->task_type |= type_flags;
  task->display = gst_mfx_task_aggregator_get_display(aggregator);
  task->session = session;
  task->aggregator = gst_mfx_task_aggregator_ref (aggregator);

  gst_mfx_task_aggregator_add_task (aggregator, task);

  MFXVideoCORE_SetHandle (task->session, MFX_HANDLE_VA_DISPLAY,
      GST_MFX_DISPLAY_VADISPLAY (task->display));

  task->memtype_is_system = FALSE;
  task->soft_reinit = FALSE;
  task->backup_num_surfaces = 0;
  task->backup_surfaces = NULL;
}

GstMfxTask *
gst_mfx_task_new (GstMfxTaskAggregator * aggregator, guint type_flags)
{
  mfxSession session;
  gboolean is_joined;

  g_return_val_if_fail (aggregator != NULL, NULL);

  session = gst_mfx_task_aggregator_create_session (aggregator, &is_joined);
  if (!session)
    return NULL;

  return
    gst_mfx_task_new_with_session (aggregator, session, type_flags, is_joined);
}

GstMfxTask *
gst_mfx_task_new_with_session (GstMfxTaskAggregator * aggregator,
    mfxSession session, guint type_flags, gboolean is_joined)
{
  GstMfxTask *task;

  g_return_val_if_fail (aggregator != NULL, NULL);
  g_return_val_if_fail (session != NULL, NULL);

  task = (GstMfxTask *) gst_mfx_mini_object_new0 (gst_mfx_task_class ());
  if (!task)
    return NULL;

  gst_mfx_task_init (task, aggregator, session, type_flags, is_joined);

  return task;
}

GstMfxTask *
gst_mfx_task_ref (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, NULL);

  return (GstMfxTask *) gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (task));
}

void
gst_mfx_task_unref (GstMfxTask * task)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (task));
}

void
gst_mfx_task_replace (GstMfxTask ** old_task_ptr, GstMfxTask * new_task)
{
  g_return_if_fail (old_task_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_task_ptr,
      GST_MFX_MINI_OBJECT (new_task));
}

void
gst_mfx_task_set_soft_reinit (GstMfxTask *task, gboolean reinit_status)
{
  g_return_if_fail (task != NULL);

  task->soft_reinit = reinit_status;
}

gboolean
gst_mfx_task_get_soft_reinit (GstMfxTask *task)
{
  g_return_val_if_fail (task != NULL, FALSE);

  return task->soft_reinit;
}
