/*
 *  Copyright (C)
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

#include "sysdeps.h"

#include "gstmfxwindow_d3d11.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxsurface.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_WINDOW_D3D11_CAST(obj) \
	((GstMfxWindowD3D11 *)(obj))

struct _GstMfxWindowD3D11
{
  /*< private > */
  GstMfxWindow parent_instance;
};

G_DEFINE_TYPE(GstMfxWindowD3D11, gst_mfx_window_d3d11, GST_TYPE_MFX_WINDOW);

static gboolean
gst_mfx_window_d3d11_render (GstMfxWindow * window,
    GstMfxSurface * surface,
    const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::render()");
  return TRUE;
}

gst_mfx_window_d3d11_show(GstMfxWindow * window)
{
  GST_WARNING("unimplemented GstMfxWindowD3D11::show()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_hide (GstMfxWindow * window)
{
  GST_FIXME ("unimplemented GstMfxWindowD3D11::hide()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_set_fullscreen (GstMfxWindow * window,
    gboolean fullscreen)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::set_fullscreen()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_create (GstMfxWindow * window,
    guint * width, guint * height)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::create()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_destroy (GObject * window)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::destroy()");
  return TRUE;
}

static gboolean
gst_mfx_window_d3d11_resize (GstMfxWindow * window, guint width, guint height)
{
  GST_FIXME("unimplemented GstMfxWindowD3D11::resize()");
  return TRUE;
}

static void
gst_mfx_window_d3d11_class_init (GstMfxWindowD3D11Class * klass)
{
  GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS (klass);

  window_class->create = gst_mfx_window_d3d11_create;
  window_class->destroy = gst_mfx_window_d3d11_destroy;
  window_class->show = gst_mfx_window_d3d11_show;
  window_class->render = gst_mfx_window_d3d11_render;
  window_class->hide = gst_mfx_window_d3d11_hide;
  window_class->resize = gst_mfx_window_d3d11_resize;
  window_class->set_fullscreen = gst_mfx_window_d3d11_set_fullscreen;
}

static void
gst_mfx_window_d3d11_init(GstMfxWindowD3D11 * window)
{
}

GstMfxWindow *
gst_mfx_window_d3d11_new (GstMfxWindowD3D11 * window, guint width, guint height)
{
  GST_DEBUG ("new window, size %ux%u", width, height);

  return gst_mfx_window_new_internal (GST_MFX_WINDOW(window),
    GST_MFX_ID_INVALID, width, height);
}
