/*
 *  Copyright (C) 2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#ifndef __GST_MFX_VIDEO_BUFFER_POOL_H__
#define __GST_MFX_VIDEO_BUFFER_POOL_H__

#include <gst/video/gstvideopool.h>

#include <gst-libs/mfx/gstmfxdisplay.h>
#include <gst-libs/mfx/gstmfxtaskaggregator.h>

G_BEGIN_DECLS

typedef struct _GstMfxVideoBufferPool           GstMfxVideoBufferPool;
typedef struct _GstMfxVideoBufferPoolClass      GstMfxVideoBufferPoolClass;
typedef struct _GstMfxVideoBufferPoolPrivate    GstMfxVideoBufferPoolPrivate;

#define GST_MFX_TYPE_VIDEO_BUFFER_POOL \
  (gst_mfx_video_buffer_pool_get_type ())
#define GST_MFX_VIDEO_BUFFER_POOL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_MFX_TYPE_VIDEO_BUFFER_POOL, \
      GstMfxVideoBufferPool))
#define GST_MFX_VIDEO_BUFFER_POOL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_MFX_TYPE_VIDEO_BUFFER_POOL, \
      GstMfxVideoBufferPoolClass))
#define GST_MFX_IS_VIDEO_BUFFER_POOL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_MFX_TYPE_VIDEO_BUFFER_POOL))
#define GST_MFX_IS_VIDEO_BUFFER_POOL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_MFX_TYPE_VIDEO_BUFFER_POOL))


#define GST_BUFFER_POOL_OPTION_MFX_VIDEO_META \
  "GstBufferPoolOptionMfxVideoMeta"

#ifndef GST_BUFFER_POOL_OPTION_DMABUF_MEMORY
#define GST_BUFFER_POOL_OPTION_DMABUF_MEMORY \
  "GstBufferPoolOptionDMABUFMemory"
#endif

struct _GstMfxVideoBufferPool
{
  GstBufferPool bufferpool;

  GstMfxVideoBufferPoolPrivate *priv;
};

struct _GstMfxVideoBufferPoolClass
{
  GstBufferPoolClass parent_instance;
};

GType gst_mfx_video_buffer_pool_get_type (void);

GstBufferPool *
gst_mfx_video_buffer_pool_new (GstMfxTaskAggregator * aggregator,
    gboolean memtype_is_system);

void
gst_mfx_video_buffer_pool_set_untiled (GstBufferPool *pool, gboolean untiled);

G_END_DECLS

#endif /* __GST_MFX_VIDEO_BUFFER_POOL_H__ */
