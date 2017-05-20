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
#include "video-format.h"
#include "gstmfxtypes.h"

#define DEBUG 1
#include "gstmfxdebug.h"

typedef struct _ResponseData ResponseData;
struct _ResponseData
{
  GstMfxMemoryId *mem_ids;
  mfxMemId *mids;
  mfxU16 num_surfaces;
  mfxFrameAllocResponse *response;
  mfxFrameInfo frame_info;
  guint num_used;
};

struct _GstMfxTask
{
  GstObject parent_instance;

  GstMfxTaskAggregator *aggregator;
  GList *saved_responses;
  mfxFrameAllocRequest request;
  mfxVideoParam params;
  mfxSession session;
  guint task_type;
  gboolean memtype_is_system;
  gboolean is_joined;
};

G_DEFINE_TYPE(GstMfxTask, gst_mfx_task, GST_TYPE_OBJECT);

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
  return MFX_ERR_UNSUPPORTED;
}

mfxStatus
gst_mfx_task_frame_free (mfxHDL pthis, mfxFrameAllocResponse * resp)
{
  return MFX_ERR_UNSUPPORTED;
}

static mfxStatus
gst_mfx_task_frame_lock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  return MFX_ERR_UNSUPPORTED;
}

static mfxStatus
gst_mfx_task_frame_unlock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  return MFX_ERR_UNSUPPORTED;
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
gst_mfx_task_finalize (GObject * object)
{
  GstMfxTask* task = GST_MFX_TASK(object);
  if (task->is_joined) {
    MFXDisjoinSession (task->session);
    MFXClose (task->session);
  }
  gst_mfx_task_aggregator_remove_task (task->aggregator, task);
  gst_mfx_task_aggregator_unref (task->aggregator);
  g_list_free_full (task->saved_responses, g_free);
}

static void
gst_mfx_task_class_init (GstMfxTaskClass * klass)
{
	GObjectClass *const object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gst_mfx_task_finalize;
}

static void
gst_mfx_task_init(GstMfxTask * task)
{
}

static void
gst_mfx_task_create (GstMfxTask * task, GstMfxTaskAggregator * aggregator,
    mfxSession session, guint type_flags, gboolean is_joined)
{
  task->is_joined = is_joined;
  task->task_type |= type_flags;
  task->session = session;
  task->aggregator = gst_mfx_task_aggregator_ref (aggregator);

  gst_mfx_task_aggregator_add_task (aggregator, task);

  task->memtype_is_system = FALSE;
}

GstMfxTask *
gst_mfx_task_new (GstMfxTask * task, GstMfxTaskAggregator * aggregator, guint type_flags)
{
  mfxSession session;
  gboolean is_joined;

  g_return_val_if_fail (task != NULL, NULL);
  g_return_val_if_fail (aggregator != NULL, NULL);

  session = gst_mfx_task_aggregator_create_session (aggregator, &is_joined);
  if (!session)
    return NULL;

  return
    gst_mfx_task_new_with_session (task, aggregator, session, type_flags, is_joined);
}

GstMfxTask *
gst_mfx_task_new_with_session (GstMfxTask * task, GstMfxTaskAggregator * aggregator,
    mfxSession session, guint type_flags, gboolean is_joined)
{
  g_return_val_if_fail (task != NULL, NULL);
  g_return_val_if_fail (aggregator != NULL, NULL);
  g_return_val_if_fail (session != NULL, NULL);

  gst_mfx_task_create (task, aggregator, session, type_flags, is_joined);

  return task;
}

GstMfxTask *
gst_mfx_task_ref (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, NULL);

  return gst_object_ref (GST_OBJECT (task));
}

void
gst_mfx_task_unref (GstMfxTask * task)
{
	gst_object_unref (GST_OBJECT(task));
}

void
gst_mfx_task_replace (GstMfxTask ** old_task_ptr, GstMfxTask * new_task)
{
  g_return_if_fail (old_task_ptr != NULL);

  gst_object_replace ((GstObject **) old_task_ptr,
      GST_OBJECT (new_task));
}


