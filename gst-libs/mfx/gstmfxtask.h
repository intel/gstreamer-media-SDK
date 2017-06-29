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

#ifndef GST_MFX_TASK_H
#define GST_MFX_TASK_H

#include "gstmfxminiobject.h"
#include "gstmfxdisplay.h"

#include <mfxvideo.h>
#include <va/va.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_MFX_TASK(obj) \
  ((GstMfxTask *) (obj))

#define GST_MFX_TASK_SESSION(task) \
  gst_mfx_task_get_session (task)

#define GST_MFX_TASK_DISPLAY(task) \
  gst_mfx_task_get_display (task)


typedef struct _GstMfxTask GstMfxTask;
typedef struct _GstMfxTaskAggregator GstMfxTaskAggregator;

typedef enum {
  GST_MFX_TASK_INVALID = 0,
  GST_MFX_TASK_DECODER = (1 << 0),
  GST_MFX_TASK_VPP_IN = (1 << 1),
  GST_MFX_TASK_VPP_OUT = (1 << 2),
  GST_MFX_TASK_ENCODER = (1 << 3),
} GstMfxTaskType;

GstMfxTask *
gst_mfx_task_new (GstMfxTaskAggregator * aggregator,
  guint type_flags);

GstMfxTask *
gst_mfx_task_new_with_session (GstMfxTaskAggregator * aggregator,
    mfxSession session, guint type_flags, gboolean is_joined);

GstMfxTask *
gst_mfx_task_ref (GstMfxTask * task);

void
gst_mfx_task_unref (GstMfxTask * task);

void
gst_mfx_task_replace (GstMfxTask ** old_task_ptr,
    GstMfxTask * new_task);

mfxFrameAllocRequest *
gst_mfx_task_get_request (GstMfxTask * task);

void
gst_mfx_task_set_request (GstMfxTask * task, mfxFrameAllocRequest * req);

gboolean
gst_mfx_task_has_type (GstMfxTask * task, guint flags);

void
gst_mfx_task_set_task_type (GstMfxTask * task, guint flags);

guint
gst_mfx_task_get_task_type (GstMfxTask * task);

void
gst_mfx_task_use_video_memory (GstMfxTask * task);

gboolean
gst_mfx_task_has_video_memory (GstMfxTask * task);

void
gst_mfx_task_set_video_params (GstMfxTask * task, mfxVideoParam * params);

mfxVideoParam *
gst_mfx_task_get_video_params (GstMfxTask * task);

void
gst_mfx_task_update_video_params (GstMfxTask * task, mfxVideoParam * params);

void
gst_mfx_task_ensure_memtype_is_system (GstMfxTask * task);

GstMfxDisplay *
gst_mfx_task_get_display (GstMfxTask * task);

GstMfxMemoryId *
gst_mfx_task_get_memory_id (GstMfxTask * task);

guint
gst_mfx_task_get_num_surfaces (GstMfxTask * task);

mfxSession
gst_mfx_task_get_session (GstMfxTask * task);

void
gst_mfx_task_set_soft_reinit (GstMfxTask * task, gboolean reinit_status);

gboolean
gst_mfx_task_get_soft_reinit (GstMfxTask * task);

/* ------------------------------------------------------------------------ */
/* --- MFX Frame Allocator                                              --- */
/* ------------------------------------------------------------------------ */

mfxStatus
gst_mfx_task_frame_alloc (mfxHDL pthis, mfxFrameAllocRequest *req,
    mfxFrameAllocResponse *resp);

mfxStatus
gst_mfx_task_frame_free (mfxHDL pthis, mfxFrameAllocResponse *resp);

G_END_DECLS

#endif /* GST_MFX_TASK_H */
