/*
 *  Copyright (C) 2012-2013 Intel Corporation
 *    Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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

#ifndef GST_MFX_DISPLAY_WAYLAND_PRIV_H
#define GST_MFX_DISPLAY_WAYLAND_PRIV_H

#include <intel_bufmgr.h>
#include "gstmfxdisplay_wayland.h"
#include "wayland-drm-client-protocol.h"
#include "scaler-client-protocol.h"

G_BEGIN_DECLS
#define GST_MFX_IS_DISPLAY_WAYLAND(display) \
  ((display) != NULL && \
  GST_MFX_DISPLAY_TYPE (display) == GST_MFX_DISPLAY_TYPE_WAYLAND)
#define GST_MFX_DISPLAY_WAYLAND_CAST(display) \
  ((GstMfxDisplayWayland *)(display))
#define GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display) \
  (&GST_MFX_DISPLAY_WAYLAND_CAST(display)->priv)
#ifndef BATCH_SIZE
#define BATCH_SIZE 0x80000
#endif

typedef struct _GstMfxDisplayWaylandPrivate GstMfxDisplayWaylandPrivate;
struct _GstMfxDisplayWaylandPrivate
{
  gchar *display_name;
  struct wl_compositor *compositor;
  struct wl_shell *shell;
  struct wl_output *output;
  struct wl_registry *registry;
  struct wl_drm *drm;
  struct wl_scaler *scaler;
  guint width;
  guint height;
  guint phys_width;
  guint phys_height;
  gint event_fd;
  gint drm_fd;
  gchar *drm_device_name;
  drm_intel_bufmgr *bufmgr;
  gboolean is_auth;
};

/**
 * GstMfxDisplayWayland:
 *
 * MFX/Wayland display wrapper.
 */
struct _GstMfxDisplayWayland
{
  /*< private > */
  GstMfxDisplay parent_instance;

  GstMfxDisplayWaylandPrivate priv;
};

G_END_DECLS
#endif /* GST_MFX_DISPLAY_WAYLAND_PRIV_H */
