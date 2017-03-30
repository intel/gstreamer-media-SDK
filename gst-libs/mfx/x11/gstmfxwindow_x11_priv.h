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

G_BEGIN_DECLS

#define GST_MFX_WINDOW_X11_GET_PRIVATE(obj) \
  (&GST_MFX_WINDOW_X11(obj)->priv)

#define GST_MFX_WINDOW_X11_CLASS(klass) \
  ((GstMfxWindowX11Class *)(klass))

#define GST_MFX_WINDOW_X11_GET_CLASS(obj) \
  GST_MFX_WINDOW_X11_CLASS(GST_MFX_WINDOW_GET_CLASS(obj))

typedef struct _GstMfxWindowX11Private GstMfxWindowX11Private;
typedef struct _GstMfxWindowX11Class GstMfxWindowX11Class;

struct _GstMfxWindowX11Private
{
  Atom atom_NET_WM_STATE;
  Atom atom_NET_WM_STATE_FULLSCREEN;

  guint is_mapped;
  guint fullscreen_on_map;
#ifdef HAVE_XRENDER
  Picture picture;
#endif
  xcb_connection_t *xcbconn;
  /* Used for thread-safe locking of XRender operations */
  GstMfxDisplay *display;
};

/**
 * GstMfxWindowX11:
 *
 * An X11 #Window wrapper.
 */
struct _GstMfxWindowX11
{
  /*< private >*/
  GstMfxWindow parent_instance;

  GstMfxWindowX11Private priv;
};

/**
 * GstMfxWindowX11Class:
 *
 * An X11 #Window wrapper class.
 */
struct _GstMfxWindowX11Class
{
  /*< private >*/
  GstMfxWindowClass parent_class;
};

void
gst_mfx_window_x11_class_init(GstMfxWindowX11Class * klass);


G_END_DECLS

#endif /* GST_MFX_WINDOW_X11_PRIV_H */
