/*
 *  Copyright (C) 2012-2013 Intel Corporation
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

#ifndef GST_MFX_WINDOW_H
#define GST_MFX_WINDOW_H

#include "gstmfxtypes.h"
#include "gstmfxcontext.h"
#include "gstmfxsurface.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_WINDOW (gst_mfx_window_get_type ())
#define GST_MFX_WINDOW(obj) ((GstMfxWindow *)(obj))

typedef struct _GstMfxWindow GstMfxWindow;

GType
gst_mfx_window_get_type (void);

GstMfxWindow *
gst_mfx_window_ref (GstMfxWindow * window);

void
gst_mfx_window_unref (GstMfxWindow * window);

void
gst_mfx_window_replace (GstMfxWindow ** old_window_ptr,
    GstMfxWindow * new_window);

GstMfxContext *
gst_mfx_window_get_context (GstMfxWindow * window);

void
gst_mfx_window_show (GstMfxWindow * window);

void
gst_mfx_window_hide (GstMfxWindow * window);

guintptr
gst_mfx_window_get_handle (GstMfxWindow * window);

gboolean
gst_mfx_window_get_fullscreen (GstMfxWindow * window);

void
gst_mfx_window_set_fullscreen (GstMfxWindow * window, gboolean fullscreen);

guint gst_mfx_window_get_width (GstMfxWindow * window);

guint gst_mfx_window_get_height (GstMfxWindow * window);

void
gst_mfx_window_get_size (GstMfxWindow * window, guint * width_ptr,
    guint * height_ptr);

void
gst_mfx_window_set_width (GstMfxWindow * window, guint width);

void
gst_mfx_window_set_height (GstMfxWindow * window, guint height);

void
gst_mfx_window_set_size (GstMfxWindow * window, guint width, guint height);

gboolean
gst_mfx_window_put_surface (GstMfxWindow * window,
    GstMfxSurface * surface, const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect);

void
gst_mfx_window_reconfigure (GstMfxWindow * window);

G_END_DECLS
#endif /* GST_MFX_WINDOW_H */
