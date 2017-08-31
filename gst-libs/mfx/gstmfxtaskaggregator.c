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

#include "gstmfxtaskaggregator.h"
#include "gstmfxcontext.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/**
* GstMfxTaskAggregator:
*
* An MFX aggregator wrapper.
*/
struct _GstMfxTaskAggregator
{
  /*< private > */
  GstObject parent_instance;

  GstMfxContext *context;
  GList *tasks;
  GstMfxTask *current_task;
  mfxSession parent_session;
  mfxVersion version;
  mfxU16 platform;
};

G_DEFINE_TYPE (GstMfxTaskAggregator, gst_mfx_task_aggregator, GST_TYPE_OBJECT);

static void
gst_mfx_task_aggregator_finalize (GObject * object)
{
  GstMfxTaskAggregator *aggregator = GST_MFX_TASK_AGGREGATOR (object);

  gst_mfx_context_replace (&aggregator->context, NULL);
  MFXClose (aggregator->parent_session);
  g_list_free (aggregator->tasks);
}

static void
gst_mfx_task_aggregator_init (GstMfxTaskAggregator * aggregator)
{
  aggregator->tasks = NULL;
  aggregator->context = NULL;
  aggregator->version.Major = GST_MFX_MIN_MSDK_VERSION_MAJOR;
  aggregator->version.Minor = GST_MFX_MIN_MSDK_VERSION_MINOR;
}

GstMfxTaskAggregator *
gst_mfx_task_aggregator_new (void)
{
  return g_object_new (GST_TYPE_MFX_TASK_AGGREGATOR, NULL);
}

GstMfxTaskAggregator *
gst_mfx_task_aggregator_ref (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, NULL);

  return gst_object_ref (GST_OBJECT (aggregator));
}

void
gst_mfx_task_aggregator_unref (GstMfxTaskAggregator * aggregator)
{
  gst_object_unref (GST_OBJECT (aggregator));
}

void
gst_mfx_task_aggregator_replace (GstMfxTaskAggregator ** old_aggregator_ptr,
    GstMfxTaskAggregator * new_aggregator)
{
  g_return_if_fail (old_aggregator_ptr != NULL);

  gst_object_replace ((GstObject **) old_aggregator_ptr,
      GST_OBJECT (new_aggregator));
}

GstMfxContext *
gst_mfx_task_aggregator_get_context (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, NULL);

  return aggregator->context ? gst_mfx_context_ref (aggregator->context) : NULL;
}

mfxSession
gst_mfx_task_aggregator_init_session_context (GstMfxTaskAggregator * aggregator,
    gboolean * is_joined)
{
  mfxInitParam init_params = { 0 };
  mfxIMPL impl;
  mfxStatus sts;
  mfxSession session = NULL;
  const char *desc;

  impl = MFX_IMPL_HARDWARE_ANY;
#if WITH_D3D11_BACKEND
  impl |= MFX_IMPL_VIA_D3D11;
#endif

  sts = MFXInit (impl, &aggregator->version, &session);
  if (sts < 0) {
    GST_ERROR ("Error initializing internal MFX session");
    return NULL;
  }

  MFXQueryVersion (session, &aggregator->version);

  GST_INFO ("Using Media SDK API version %d.%d",
      aggregator->version.Major, aggregator->version.Minor);

  MFXQueryIMPL (session, &impl);

  switch (MFX_IMPL_BASETYPE (impl)) {
    case MFX_IMPL_SOFTWARE:
      desc = "software";
      break;
    case MFX_IMPL_HARDWARE:
    case MFX_IMPL_HARDWARE2:
    case MFX_IMPL_HARDWARE3:
    case MFX_IMPL_HARDWARE4:
      desc = "hardware accelerated";
      break;
    default:
      desc = "unknown";
  }

  GST_INFO ("Initialized internal MFX session using %s implementation", desc);

  if (!aggregator->parent_session) {
    aggregator->parent_session = session;
    *is_joined = FALSE;
  } else {
    sts = MFXJoinSession (aggregator->parent_session, session);
    *is_joined = TRUE;
  }

  if (!aggregator->context)
    aggregator->context = gst_mfx_context_new (aggregator->parent_session);

  return session;
}

GstMfxTask *
gst_mfx_task_aggregator_get_current_task (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, NULL);

  return aggregator->current_task;
}

void
gst_mfx_task_aggregator_set_current_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task)
{
  g_return_if_fail (aggregator != NULL);
  g_return_if_fail (task != NULL);

  aggregator->current_task = task;
}

GstMfxTask *
gst_mfx_task_aggregator_get_last_task (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, NULL);

  GList *l = g_list_first (aggregator->tasks);
  return l ? gst_mfx_task_ref (GST_MFX_TASK (l->data)) : NULL;
}

void
gst_mfx_task_aggregator_add_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task)
{
  g_return_if_fail (aggregator != NULL);
  g_return_if_fail (task != NULL);

  aggregator->tasks = g_list_prepend (aggregator->tasks, task);
}

void
gst_mfx_task_aggregator_remove_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task)
{
  GList *elem;

  g_return_if_fail (aggregator != NULL);
  g_return_if_fail (task != NULL);

  elem = g_list_find (aggregator->tasks, task);
  if (!elem)
    return;

  aggregator->tasks = g_list_delete_link (aggregator->tasks, elem);
}

void
gst_mfx_task_aggregator_update_peer_memtypes (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task, gboolean memtype_is_system)
{
  GstMfxTask *upstream_task;
  guint task_index;
  mfxVideoParam *params;

  g_return_if_fail (aggregator != NULL);
  g_return_if_fail (task != NULL);

  task_index = g_list_index (aggregator->tasks, task);

  do {
    upstream_task = g_list_nth_data (aggregator->tasks, task_index++);
    if (!upstream_task)
      break;
    params = gst_mfx_task_get_video_params (upstream_task);
    if (gst_mfx_task_has_type (upstream_task, GST_MFX_TASK_VPP_OUT)) {
      if (memtype_is_system) {
        params->IOPattern =
            MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
      } else {
        params->IOPattern &=
            MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY;
        params->IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
      }
    } else if (gst_mfx_task_has_type (upstream_task, GST_MFX_TASK_DECODER)) {
      params->IOPattern = memtype_is_system ?
          MFX_IOPATTERN_OUT_SYSTEM_MEMORY : MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    memtype_is_system = ! !(params->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY);
  } while (memtype_is_system);
}

#if MSDK_CHECK_VERSION(1,19)
mfxU16
gst_mfx_task_aggregator_get_platform (GstMfxTaskAggregator * aggregator)
{
  g_return_val_if_fail (aggregator != NULL, MFX_PLATFORM_UNKNOWN);

  if (!aggregator->platform) {
    mfxPlatform platform = { 0 };
    mfxStatus sts =
        MFXVideoCORE_QueryPlatform (aggregator->parent_session, &platform);
    if (MFX_ERR_NONE == sts) {
      aggregator->platform = platform.CodeName;
      GST_INFO ("Detected MFX platform with device code %d",
          aggregator->platform);
    }
    else {
      GST_WARNING ("Platform autodetection failed with MFX status %d", sts);
    }
  }

  return aggregator->platform;
}
#endif

static void
gst_mfx_task_aggregator_class_init (GstMfxTaskAggregatorClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
      "mfxtaskaggregator", 0, "MFX Context");
  object_class->finalize = gst_mfx_task_aggregator_finalize;
}
