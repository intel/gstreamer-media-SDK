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

#ifndef SYSDEPS_H
#define SYSDEPS_H

#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_LIBVA_BACKEND
# include <unistd.h>
# include <va/va.h>
# include <drm.h>
#else
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0600
# endif
# define COBJMACROS

# include <initguid.h>
# include <d3d11.h>
# include <dxgi1_2.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <mfxvideo.h>

/* Media SDK API version check  */
#define	MSDK_CHECK_VERSION(major,minor)	\
    (MFX_VERSION_MAJOR > major || \
     (MFX_VERSION_MAJOR == major && MFX_VERSION_MINOR > minor) || \
     (MFX_VERSION_MAJOR == major && MFX_VERSION_MINOR == minor))

/* Session APIs used required 1.1, MFXInitEx requires 1.14 */
#define GST_MFX_MIN_MSDK_VERSION_MAJOR 1
#define GST_MFX_MIN_MSDK_VERSION_MINOR 14

#endif /* SYSDEPS_H */
