/*
 *  Copyright (C) 2011-2014 Intel Corporation
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

#ifndef GST_MFXSINK_H
#define GST_MFXSINK_H

#include "gstmfxpluginbase.h"
#include "gstmfxpluginutil.h"

#include <gst-libs/mfx/gstmfxdisplay.h>
#include <gst-libs/mfx/gstmfxwindow.h>
#include <gst-libs/mfx/gstmfxcompositefilter.h>

G_BEGIN_DECLS

#define GST_TYPE_MFXSINK \
  (gst_mfxsink_get_type ())
#define GST_MFXSINK_CAST(obj) \
  ((GstMfxSink *)(obj))
#define GST_MFXSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXSINK, GstMfxSink))
#define GST_MFXSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXSINK, GstMfxSinkClass))
#define GST_IS_MFXSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXSINK))
#define GST_IS_MFXSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXSINK))
#define GST_MFXSINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXSINK, GstMfxSinkClass))

typedef struct _GstMfxSink                    GstMfxSink;
typedef struct _GstMfxSinkClass               GstMfxSinkClass;
typedef struct _GstMfxSinkBackend             GstMfxSinkBackend;

typedef gboolean(*GstMfxSinkCreateWindowFunc) (GstMfxSink * sink,
    guint width, guint height);
typedef gboolean(*GstMfxSinkCreateWindowFromHandleFunc) (GstMfxSink * sink,
    guintptr window);
typedef gboolean(*GstMfxSinkHandleEventsFunc) (GstMfxSink * sink);
typedef gboolean(*GstMfxSinkPreStartEventThreadFunc) (GstMfxSink * sink);
typedef gboolean(*GstMfxSinkPreStopEventThreadFunc) (GstMfxSink * sink);

typedef enum {
  GST_MFX_GLAPI_OPENGL = 0,
  GST_MFX_GLAPI_GLES2 = 2,
} GstMfxGLAPI;

struct _GstMfxSinkBackend
{
  GstMfxSinkCreateWindowFunc              create_window;
  GstMfxSinkCreateWindowFromHandleFunc    create_window_from_handle;

  /* Event threads handling */
  GstMfxSinkHandleEventsFunc              handle_events;
  GstMfxSinkPreStartEventThreadFunc       pre_start_event_thread;
  GstMfxSinkPreStopEventThreadFunc        pre_stop_event_thread;
};

struct _GstMfxSink
{
  /*< private >*/
  GstMfxPluginBase           parent_instance;

  const GstMfxSinkBackend   *backend;

  GstCaps                   *caps;
  GstMfxWindow              *window;
  guint                      window_width;
  guint                      window_height;
  guint                      video_width;
  guint                      video_height;
  gint                       video_par_n;
  gint                       video_par_d;
  GstVideoInfo               video_info;
  GstMfxRectangle            display_rect;
  GThread                   *event_thread;
  volatile gboolean          event_thread_cancel;

  GstMfxCompositeFilter     *composite_filter;
  GstMfxDisplay             *drm_display;

  GstMfxDisplay             *display;
  GstMfxDisplayType          display_type;
  GstMfxDisplayType          display_type_req;
  gchar                     *display_name;

  GstMfxGLAPI                gl_api;

  guint                      handle_events : 1;
  guint                      foreign_window : 1;
  guint                      fullscreen : 1;
  guint                      keep_aspect : 1;
  guint                      no_frame_drop : 1;
  guint                      full_color_range : 1;

  guintptr                   app_window_handle;
};

struct _GstMfxSinkClass
{
  /*< private >*/
  GstMfxPluginBaseClass parent_class;
};

GType
gst_mfxsink_get_type(void);

G_END_DECLS


#endif /* GST_MFXSINK_H */
