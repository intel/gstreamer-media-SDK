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

#ifndef GST_MFX_WINDOW_X11_H
#define GST_MFX_WINDOW_X11_H

#include <X11/Xlib.h>
#include "gstmfxdisplay.h"
#include "gstmfxwindow.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW_X11(obj) \
	((GstMfxWindowX11 *)(obj))

/**
* GST_MFX_WINDOW_XWINDOW:
* @window: a #GstMfxWindow
*
* Macro that evaluates to the underlying X11 #Window of @window
*/
#define GST_MFX_WINDOW_XWINDOW(window) \
	gst_mfx_window_x11_get_xid(GST_MFX_WINDOW_X11(window))

typedef struct _GstMfxWindowX11 GstMfxWindowX11;

GstMfxWindow *
gst_mfx_window_x11_new(GstMfxDisplay * display, guint width, guint height);

Window
gst_mfx_window_x11_get_xid(GstMfxWindowX11 * window);

G_END_DECLS

#endif /* GST_MFX_WINDOW_X11_H */
