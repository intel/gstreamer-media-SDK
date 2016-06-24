/*
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

#ifndef GST_MFX_DISPLAY_DRM_PRIV_H
#define GST_MFX_DISPLAY_DRM_PRIV_H

#include "gstmfxdisplay_drm.h"
#include "gstmfxdisplay_priv.h"

G_BEGIN_DECLS

#define GST_MFX_IS_DISPLAY_DRM(display) \
  ((display) != NULL && \
  GST_MFX_DISPLAY_VADISPLAY_TYPE (display) == GST_MFX_DISPLAY_TYPE_DRM)

#define GST_MFX_DISPLAY_DRM_CAST(display) \
  ((GstMfxDisplayDRM *)(display))

#define GST_MFX_DISPLAY_DRM_PRIVATE(display) \
  (&GST_MFX_DISPLAY_DRM_CAST(display)->priv)

typedef struct _GstMfxDisplayDRMPrivate       GstMfxDisplayDRMPrivate;
typedef struct _GstMfxDisplayDRMClass         GstMfxDisplayDRMClass;

/**
 * GST_MFX_DISPLAY_DRM_DEVICE
 * @display: a #GstMfxDisplay
 *
 * Macro that evaluated to the underlying DRM file descriptor of @display
 */
#undef  GST_MFX_DISPLAY_DRM_DEVICE
#define GST_MFX_DISPLAY_DRM_DEVICE(display) \
    GST_MFX_DISPLAY_DRM_PRIVATE(display)->drm_device

struct _GstMfxDisplayDRMPrivate
{
    //gchar *device_path_default;
    gchar *device_path;
    gint drm_device;
    guint use_foreign_display:1;
};

/**
* GstMfxDisplayDRM:
*
* VA/DRM display wrapper.
*/
struct _GstMfxDisplayDRM
{
  /*< private >*/
  GstMfxDisplay parent_instance;

  GstMfxDisplayDRMPrivate priv;
};

/**
* GstMfxDisplayDRMClass:
*
* VA/X11 display wrapper clas.
*/
struct _GstMfxDisplayDRMClass
{
  /*< private >*/
  GstMfxDisplayClass parent_class;
};


G_END_DECLS

#endif /* GST_MFX_DISPLAY_DRM_PRIV_H */
