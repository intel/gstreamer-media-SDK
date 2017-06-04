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
#include "gstmfxtask_priv.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxallocator_d3d11.h"

#define DEBUG 1
#include "gstmfxdebug.h"

G_DEFINE_TYPE(GstMfxTask, gst_mfx_task, GST_TYPE_OBJECT);

mfxSession
gst_mfx_task_get_session (GstMfxTask * task)
{
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  g_return_val_if_fail (task != NULL, 0);

  return GST_MFX_TASK_GET_PRIVATE(task)->session;
}

GstMfxMemoryId *
gst_mfx_task_get_memory_id (GstMfxTask * task)
{
  GList *l;
  ResponseData *response_data;

  g_return_val_if_fail (task != NULL, 0);

  l = g_list_first (GST_MFX_TASK_GET_PRIVATE(task)->saved_responses);
  response_data = l->data;

  return response_data->mids[response_data->num_used++];
}

guint
gst_mfx_task_get_num_surfaces (GstMfxTask * task)
{
  GList *l;
  ResponseData *response_data;

  g_return_val_if_fail (task != NULL, 0);

  l = g_list_first (GST_MFX_TASK_GET_PRIVATE(task)->saved_responses);
  response_data = l->data;

  return response_data->num_surfaces;
}

mfxFrameAllocRequest *
gst_mfx_task_get_request (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, NULL);

  return &GST_MFX_TASK_GET_PRIVATE(task)->request;
}

void
gst_mfx_task_set_request (GstMfxTask * task, mfxFrameAllocRequest * request)
{
  g_return_if_fail (task != NULL);

  GST_MFX_TASK_GET_PRIVATE(task)->request = *request;
}

gboolean
gst_mfx_task_has_type (GstMfxTask * task, guint flags)
{
  g_return_val_if_fail (task != NULL, FALSE);

  return (GST_MFX_TASK_GET_PRIVATE(task)->task_type & flags);
}

void
gst_mfx_task_set_task_type (GstMfxTask * task, guint flags)
{
  g_return_if_fail (task != NULL);

  GST_MFX_TASK_GET_PRIVATE(task)->task_type = flags;
}

guint
gst_mfx_task_get_task_type (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, GST_MFX_TASK_INVALID);

  return GST_MFX_TASK_GET_PRIVATE(task)->task_type;
}

void
gst_mfx_task_ensure_memtype_is_system (GstMfxTask * task)
{
  g_return_if_fail (task != NULL);

  GST_MFX_TASK_GET_PRIVATE(task)->memtype_is_system = TRUE;
}

GstMfxContext *
gst_mfx_task_get_context (GstMfxTask * task)
{
  g_return_val_if_fail(task != NULL, NULL);

  return gst_mfx_context_ref (GST_MFX_TASK_GET_PRIVATE(task)->context);
}

void
gst_mfx_task_use_video_memory (GstMfxTask * task)
{
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  mfxFrameAllocator frame_allocator = {
    .pthis = task,
    .Alloc = gst_mfx_task_frame_alloc,
    .Lock = gst_mfx_task_frame_lock,
    .Unlock = gst_mfx_task_frame_unlock,
    .Free = gst_mfx_task_frame_free,
    .GetHDL = gst_mfx_task_frame_get_hdl,
  };

  MFXVideoCORE_SetFrameAllocator (priv->session, &frame_allocator);
  priv->memtype_is_system = FALSE;
}

gboolean
gst_mfx_task_has_video_memory (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, FALSE);

  return !GST_MFX_TASK_GET_PRIVATE(task)->memtype_is_system;
}

void
gst_mfx_task_set_video_params (GstMfxTask * task, mfxVideoParam * params)
{
  g_return_if_fail (task != NULL);

  GST_MFX_TASK_GET_PRIVATE(task)->params = *params;
}

mfxVideoParam *
gst_mfx_task_get_video_params (GstMfxTask * task)
{
  g_return_val_if_fail (task != NULL, NULL);

  return &GST_MFX_TASK_GET_PRIVATE(task)->params;
}

void
gst_mfx_task_update_video_params (GstMfxTask * task, mfxVideoParam * params)
{
  params->AsyncDepth = GST_MFX_TASK_GET_PRIVATE(task)->params.AsyncDepth;
  params->IOPattern = GST_MFX_TASK_GET_PRIVATE(task)->params.IOPattern;
}

static void
gst_mfx_task_finalize (GObject * object)
{
  GstMfxTask* task = GST_MFX_TASK(object);
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  if (priv->is_joined) {
    MFXDisjoinSession (priv->session);
    MFXClose (priv->session);
  }
  gst_mfx_task_aggregator_remove_task (priv->aggregator, task);
  gst_mfx_task_aggregator_unref (priv->aggregator);
  gst_mfx_context_unref(priv->context);
  g_list_free_full (priv->saved_responses, g_free);
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

static gboolean
gst_mfx_task_create (GstMfxTask * task, GstMfxTaskAggregator * aggregator,
    mfxSession session, guint type_flags, gboolean is_joined)
{
  GstMfxTaskPrivate *const priv = GST_MFX_TASK_GET_PRIVATE(task);
  mfxHDL device_handle = NULL;
  mfxStatus sts = MFX_ERR_NONE;

  priv->is_joined = is_joined;
  priv->task_type |= type_flags;
  priv->session = session;
  priv->aggregator = gst_mfx_task_aggregator_ref (aggregator);
  priv->context = gst_mfx_task_aggregator_get_context(aggregator);
  priv->memtype_is_system = FALSE;

  device_handle =
      gst_mfx_device_get_handle(gst_mfx_context_get_device(priv->context));
  sts = MFXVideoCORE_GetHandle(priv->session, MFX_HANDLE_D3D11_DEVICE,
          &device_handle);
  if (MFX_ERR_NONE != sts) {
    sts = MFXVideoCORE_SetHandle(priv->session, MFX_HANDLE_D3D11_DEVICE,
            device_handle);
    if (MFX_ERR_NONE != sts)
      return FALSE;
  }

  gst_mfx_task_aggregator_add_task (aggregator, task);
  return TRUE;
}

GstMfxTask *
gst_mfx_task_new (GstMfxTask * task, GstMfxTaskAggregator * aggregator,
  guint type_flags)
{
  mfxSession session;
  gboolean is_joined;

  g_return_val_if_fail (task != NULL, NULL);
  g_return_val_if_fail (aggregator != NULL, NULL);

  session =
      gst_mfx_task_aggregator_init_session_context (aggregator, &is_joined);
  if (!session)
    return NULL;

  return
    gst_mfx_task_new_with_session (task, aggregator, session,
      type_flags, is_joined);
}

GstMfxTask *
gst_mfx_task_new_with_session (GstMfxTask * task,
  GstMfxTaskAggregator * aggregator, mfxSession session,
  guint type_flags, gboolean is_joined)
{
  g_return_val_if_fail (task != NULL, NULL);
  g_return_val_if_fail (aggregator != NULL, NULL);
  g_return_val_if_fail (session != NULL, NULL);

  if (!gst_mfx_task_create(task, aggregator, session, type_flags, is_joined))
    return NULL;

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


