/*
 *  Copyright (C) 2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) egl later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT EGL WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef GST_MFX_DISPLAY_EGL_H
#define GST_MFX_DISPLAY_EGL_H

#include <EGL/egl.h>
#include "gstmfxdisplay.h"

G_BEGIN_DECLS

typedef struct _GstMfxDisplayEGL GstMfxDisplayEGL;

#define GST_MFX_DISPLAY_EGL(obj) ((GstMfxDisplayEGL *)(obj))

GstMfxDisplay *
gst_mfx_display_egl_new (const gchar * display_name, guint gles_version);

GstMfxDisplay *
gst_mfx_display_egl_get_parent_display (GstMfxDisplayEGL * display);

G_END_DECLS

#endif /* GST_MFX_DISPLAY_EGL_H */
