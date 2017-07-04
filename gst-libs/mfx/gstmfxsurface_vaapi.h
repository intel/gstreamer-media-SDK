/*
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

#ifndef GST_MFX_SURFACE_VAAPI_H
#define GST_MFX_SURFACE_VAAPI_H

#include "gstmfxsurface.h"
#include "gstmfxsurface_priv.h"
#include "gstmfxutils_vaapi.h"
#include "gstmfxtask.h"
#include "gstmfxcontext.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_SURFACE_VAAPI (gst_mfx_surface_vaapi_get_type ())
G_DECLARE_FINAL_TYPE(GstMfxSurfaceVaapi, gst_mfx_surface_vaapi, GST_MFX, SURFACE_VAAPI, GstMfxSurface)

#define GST_MFX_SURFACE_VAAPI_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFX_SURFACE_VAAPI, \
  GstMfxSurfaceVaapiClass))

GstMfxSurface *
gst_mfx_surface_vaapi_new (GstMfxContext * context, const GstVideoInfo * info);

GstMfxSurface *
gst_mfx_surface_vaapi_new_from_task(GstMfxTask * task);

VaapiImage *
gst_mfx_surface_vaapi_derive_image(GstMfxSurface * surface);

GstMfxDisplay *
gst_mfx_surface_vaapi_get_display(GstMfxSurface * surface);

G_END_DECLS

#endif /* GST_MFX_SURFACE_VAAPI_H */

