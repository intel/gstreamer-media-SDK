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

#ifndef GST_MFX_TASK_AGGREGATOR_H
#define GST_MFX_TASK_AGGREGATOR_H

#include "sysdeps.h"
#include "gstmfxminiobject.h"
#include "gstmfxdisplay.h"
#include "gstmfxtask.h"

#include <mfxvideo.h>
#include <va/va.h>

G_BEGIN_DECLS

#define GST_MFX_TASK_AGGREGATOR (obj) \
  ((GstMfxTaskAggregator *) (obj))


GstMfxTaskAggregator *
gst_mfx_task_aggregator_new (void);

GstMfxTask *
gst_mfx_task_aggregator_get_current_task (GstMfxTaskAggregator * aggregator);

gboolean
gst_mfx_task_aggregator_set_current_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task);

void
gst_mfx_task_aggregator_remove_current_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task);

void
gst_mfx_task_aggregator_add_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task);

GstMfxTaskAggregator *
gst_mfx_task_aggregator_ref (GstMfxTaskAggregator * aggregator);

void
gst_mfx_task_aggregator_unref (GstMfxTaskAggregator * aggregator);

void
gst_mfx_task_aggregator_replace (GstMfxTaskAggregator ** old_aggregator_ptr,
    GstMfxTaskAggregator * new_aggregator);

GstMfxDisplay *
gst_mfx_task_aggregator_get_display (GstMfxTaskAggregator * aggregator);

mfxSession
gst_mfx_task_aggregator_create_session (GstMfxTaskAggregator * aggregator,
    gboolean * is_joined);

void
gst_mfx_task_aggregator_remove_task (GstMfxTaskAggregator * aggregator,
    GstMfxTask * task);

void
gst_mfx_task_aggregator_update_peer_memtypes (GstMfxTaskAggregator * aggregator,
    gboolean memtype_is_system);


G_END_DECLS

#endif /* GST_MFX_TASK_AGGREGATOR_H */
