#include <gst/gst.h>

#include "gstmfxdec.h"
#include "gstmfxsink.h"

#define PACKAGE "gstmfx"
#define VERSION "0.0.1"

static gboolean
plugin_init(GstPlugin * plugin)
{
	gst_element_register(plugin, "mfxdecode", GST_RANK_NONE,
		GST_TYPE_MFXDEC);
	gst_element_register(plugin, "mfxsink", GST_RANK_NONE,
		GST_TYPE_MFXSINK);

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
