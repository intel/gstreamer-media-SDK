#ifndef GST_MFX_UTILS_H265_H
#define GST_MFX_UTILS_H265_H

#include <gst/gstvalue.h>
#include <mfxvideo.h>

G_BEGIN_DECLS

/* Returns MFX profile from a string representation */
mfxU16
gst_mfx_utils_h265_get_profile_from_string(const gchar * str);

/* Returns a string representation for the supplied H.265 profile */
const gchar *
gst_mfx_utils_h265_get_profile_string(mfxU16 profile);

G_END_DECLS

#endif /* GST_MFX_UTILS_H265_H */