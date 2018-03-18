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

#ifndef GST_MFX_WINDOW_D3D11_H
#define GST_MFX_WINDOW_D3D11_H

#include "gstmfxwindow.h"
#include "gstmfxwindow_priv.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_WINDOW_D3D11 (gst_mfx_window_d3d11_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxWindowD3D11, gst_mfx_window_d3d11, GST_MFX,
    WINDOW_D3D11, GstMfxWindow)

GstMfxWindow *
gst_mfx_window_d3d11_new (GstMfxContext * context,
    GstVideoInfo * info, gboolean keep_aspect, gboolean fullscreen);

G_END_DECLS
#endif /* GST_MFX_WINDOW_D3D11_H */
