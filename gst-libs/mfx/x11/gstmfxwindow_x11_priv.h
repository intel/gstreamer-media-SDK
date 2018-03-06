/*
 *  Copyright (C) 2010-2011 Splitted-Desktop Systems
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@splitted-desktop.com>
 *  Copyright (C) 2012-2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#ifndef GST_MFX_WINDOW_X11_PRIV_H
#define GST_MFX_WINDOW_X11_PRIV_H

#include "gstmfxwindow_priv.h"
#include "gstmfxdisplay_x11.h"

G_BEGIN_DECLS
#define GST_MFX_IS_WINDOW_X11(window) \
  ((window) != NULL && \
  GST_MFX_WINDOW_TYPE (window) == GST_MFX_WINDOW_TYPE_X11)
#define GST_MFX_WINDOW_X11_CAST(window) \
  ((GstMfxWindowX11 *)(window))
#define GST_MFX_WINDOW_X11_GET_PRIVATE(window) \
  (&GST_MFX_WINDOW_X11_CAST(window)->priv)

typedef struct _GstMfxWindowX11Private GstMfxWindowX11Private;
struct _GstMfxWindowX11Private
{
  GstMfxDisplayX11 *display;
  GstMfxSurface *mapped_surface;

  Atom atom_NET_WM_STATE;
  Atom atom_NET_WM_STATE_FULLSCREEN;

  guint is_mapped;
  guint fullscreen_on_map;
  Picture picture;
  xcb_connection_t *xcbconn;
};

/**
 * GstMfxWindowX11:
 *
 * An X11 #Window wrapper.
 */
struct _GstMfxWindowX11
{
  /*< private > */
  GstMfxWindow parent_instance;

  GstMfxWindowX11Private priv;
};

G_DEFINE_TYPE (GstMfxWindowX11, gst_mfx_window_x11, GST_TYPE_MFX_WINDOW);

struct _GstMfxWindowX11Class
{
  /*< private > */
  GstMfxWindowClass parent_class;
};

G_END_DECLS
#endif /* GST_MFX_WINDOW_X11_PRIV_H */
