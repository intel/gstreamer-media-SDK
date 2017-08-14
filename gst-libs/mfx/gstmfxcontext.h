/*
 *  Copyright (C) 2017
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

#ifndef GST_MFX_CONTEXT_H
#define GST_MFX_CONTEXT_H

#ifdef WITH_LIBVA_BACKEND
# include "gstmfxdisplay.h"
#else
# include "gstmfxd3d11device.h"
#endif

G_BEGIN_DECLS

#define GST_TYPE_MFX_CONTEXT (gst_mfx_context_get_type ())
G_DECLARE_FINAL_TYPE(GstMfxContext, gst_mfx_context, GST_MFX, CONTEXT, GstObject)

#define GST_MFX_CONTEXT(obj) ((GstMfxContext *) (obj))

GstMfxContext *
gst_mfx_context_new (mfxSession session);

GstMfxContext *
gst_mfx_context_ref (GstMfxContext * context);

void
gst_mfx_context_unref (GstMfxContext * context);

void
gst_mfx_context_replace (GstMfxContext ** old_context_ptr,
    GstMfxContext * new_context);

#ifdef WITH_LIBVA_BACKEND
GstMfxDisplay *
#else
GstMfxD3D11Device *
#endif // WITH_LIBVA_BACKEND
gst_mfx_context_get_device (GstMfxContext * context);

void
gst_mfx_context_lock (GstMfxContext * context);

void
gst_mfx_context_unlock (GstMfxContext * context);

G_END_DECLS

#endif /* GST_MFX_CONTEXT_H */
