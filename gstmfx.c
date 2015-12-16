/*
 ============================================================================
 Name        : gst-mfx.c
 Author      : Heiher <admin@heiher.info>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2013 everyone.
 Description : 
 ============================================================================
 */

#include <gst/gst.h>

#include "gstmfxdec.h"
//#include "gstmfxenc.h"
//#include "gstmfxvpp.h"

#define PACKAGE "gstmfx"
#define VERSION "0.0.1"

static gboolean
plugin_init(GstPlugin * plugin)
{
#ifdef HAVE_MFX_DECODER
	gst_element_register(plugin, "mfxdecode", GST_RANK_NONE,
		GST_TYPE_MFXDEC);
#endif

#ifdef HAVE_MFX_ENCODER
	gst_element_register(plugin, "mfxencode", GST_RANK_NONE,
		gst_mfxenc_get_type());
#endif

#ifdef HAVE_MFX_VPP
	gst_element_register(plugin, "mfxvpp", GST_RANK_NONE,
		gst_mfxvpp_get_type());
#endif

	return TRUE;
}

GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,  /* major */
	GST_VERSION_MINOR,  /* minor */
	mfx,              /* short unique name */
	"MFX encoder/decoder/video post-processing plugins",  /* info */
	plugin_init,    /* GstPlugin::plugin_init */
	VERSION,        /* version */
	"LGPL",          /* license */
	PACKAGE,        /* package-name, usually the file archive name */
	"http://www.intel.com.my" /* origin */
	)
