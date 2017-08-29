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

#ifndef GST_MFX_DISPLAY_X11_H
#define GST_MFX_DISPLAY_X11_H

#include "gstmfxdisplay.h"
#include "gstmfxdisplay_priv.h"
#include <X11/Xlib.h>

G_BEGIN_DECLS
#define GST_TYPE_MFX_DISPLAY_X11 (gst_mfx_display_x11_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxDisplayX11, gst_mfx_display_x11, GST_MFX,
    DISPLAY_X11, GstMfxDisplay)
#define GST_MFX_DISPLAY_X11_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFX_DISPLAY_X11, \
  GstMfxDisplayX11Class))

GstMfxDisplay *
gst_mfx_display_x11_new (GstMfxDisplayX11 * display,
    const gchar * display_name);

Display *gst_mfx_display_x11_get_display (GstMfxDisplay * display);

G_END_DECLS
#endif /* GST_MFX_DISPLAY_X11_H */
