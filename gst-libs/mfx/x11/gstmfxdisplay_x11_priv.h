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

#ifndef GST_MFX_DISPLAY_X11_PRIV_H
#define GST_MFX_DISPLAY_X11_PRIV_H

#include "gstmfxdisplay_x11.h"

G_BEGIN_DECLS
#define GST_MFX_IS_DISPLAY_X11(display) \
  ((display) != NULL && \
  GST_MFX_DISPLAY_TYPE (display) == GST_MFX_DISPLAY_TYPE_X11)
#define GST_MFX_DISPLAY_X11_CAST(display) \
  ((GstMfxDisplayX11 *)(display))
#define GST_MFX_DISPLAY_X11_PRIVATE(display) \
  (&GST_MFX_DISPLAY_X11_CAST(display)->priv)

typedef struct _GstMfxDisplayX11Private GstMfxDisplayX11Private;
struct _GstMfxDisplayX11Private
{
  gchar *display_name;
  int x11_screen;
  guint use_xrandr:1;
};

/**
 * GstMfxDisplayX11:
 *
 * VA/X11 display wrapper.
 */
struct _GstMfxDisplayX11
{
  /*< private > */
  GstMfxDisplay parent_instance;

  GstMfxDisplayX11Private priv;
};


G_END_DECLS
#endif /* GST_MFX_DISPLAY_X11_PRIV_H */
