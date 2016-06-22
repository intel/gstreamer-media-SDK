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

#include <gst/gst.h>

#include "gstmfxdec.h"
#include "gstmfxpostproc.h"
#include "gstmfxsink.h"
#include "gstmfxenc_h264.h"
#include "gstmfxenc_h265.h"
#include "gstmfxenc_mpeg2.h"

#define PACKAGE "gstmfx"
#define VERSION "0.0.1"

static gboolean
plugin_init (GstPlugin * plugin)
{
	gst_element_register (plugin, "mfxdecode", GST_RANK_NONE,
		GST_TYPE_MFXDEC);
    gst_element_register (plugin, "mfxvpp", GST_RANK_NONE,
		GST_TYPE_MFXPOSTPROC);
	gst_element_register (plugin, "mfxsink", GST_RANK_NONE,
		GST_TYPE_MFXSINK);
    gst_element_register (plugin, "mfxh264enc", GST_RANK_NONE,
		GST_TYPE_MFXENC_H264);
    gst_element_register (plugin, "mfxhevcenc", GST_RANK_NONE,
		GST_TYPE_MFXENC_H265);
    gst_element_register (plugin, "mfxmpeg2enc", GST_RANK_NONE,
		GST_TYPE_MFXENC_MPEG2);

	return TRUE;
}

GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,  /* major */
	GST_VERSION_MINOR,  /* minor */
	mfx,              /* short unique name */
	"MFX encoder/decoder/video post-processing plugins",  /* info */
	plugin_init,    /* GstPlugin::plugin_init */
	VERSION,        /* version */
	"LGPL",          /* license */
	PACKAGE,        /* package-name, usually the file archive name */
	"http://www.intel.com" /* origin */
	)
