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

#ifndef GST_MFX_TASK_PRIV_H
#define GST_MFX_TASK_PRIV_H

#include "gstmfxtypes.h"
#ifdef WITH_LIBVA_BACKEND
# include "gstmfxutils_vaapi.h"
#endif // WITH_LIBVA_BACKEND

G_BEGIN_DECLS

#define GST_MFX_TASK_CAST(task) \
  ((GstMfxTask *)(task))
#define GST_MFX_TASK_GET_PRIVATE(task) \
  (&GST_MFX_TASK_CAST(task)->priv)

typedef struct _GstMfxTaskPrivate GstMfxTaskPrivate;
typedef struct _ResponseData ResponseData;

struct _ResponseData
{
#ifdef WITH_LIBVA_BACKEND
  GstMfxMemoryId *mem_ids;
  mfxMemId *mids;
  VASurfaceID *surfaces;
  VABufferID *coded_buf;
#else
  GstMfxMemoryId **mids;
#endif                          // WITH_LIBVA_BACKEND
  mfxU16 num_surfaces;
  mfxFrameAllocResponse response;
  mfxFrameInfo frame_info;
  guint num_used;
};

struct _GstMfxTaskPrivate
{
  GstMfxTaskAggregator *aggregator;
  GstMfxContext *context;
  GList *saved_responses;
  mfxFrameAllocRequest request;
  mfxVideoParam params;
  mfxSession session;
  guint task_type;
  gboolean memtype_is_system;
  gboolean is_joined;
};

struct _GstMfxTask
{
  /*< private > */
  GstObject parent_instance;

  GstMfxTaskPrivate priv;
};

G_END_DECLS
#endif /* GST_MFX_TASK_PRIV_H_PRIV_H */
