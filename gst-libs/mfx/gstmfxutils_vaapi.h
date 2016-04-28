#ifndef GST_MFX_UTILS_VAAPI_H
#define GST_MFX_UTILS_VAAPI_H

#include "gstmfxobject.h"
#include "gstmfxdisplay.h"
#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct _VaapiImage      VaapiImage;

VaapiImage *
vaapi_image_new(GstMfxDisplay * display, guint width, guint height,
    GstVideoFormat format);

VaapiImage *
vaapi_image_new_with_image(GstMfxDisplay *display, VAImage *va_image);

VaapiImage *
vaapi_image_ref(VaapiImage * image);

void
vaapi_image_unref(VaapiImage * image);

void
vaapi_image_replace(VaapiImage ** old_image_ptr,
        VaapiImage * new_image);

VAImageID
vaapi_image_get_id(VaapiImage *image);

gboolean
vaapi_image_get_image(VaapiImage *image, VAImage *va_image);

GstVideoFormat
vaapi_image_get_format(VaapiImage *image);

guint
vaapi_image_get_width(VaapiImage *image);

guint
vaapi_image_get_height(VaapiImage *image);

void
vaapi_image_get_size(VaapiImage *image, guint *pwidth, guint *pheight);

gboolean
vaapi_image_map(VaapiImage *image);

gboolean
vaapi_image_unmap(VaapiImage *image);

guint
vaapi_image_get_plane_count(VaapiImage *image);

guchar *
vaapi_image_get_plane(VaapiImage *image, guint plane);

guint
vaapi_image_get_pitch(VaapiImage *image, guint plane);

guint
vaapi_image_get_offset(VaapiImage *image, guint plane);

guint
vaapi_image_get_data_size(VaapiImage *image);

gboolean
vaapi_check_status (VAStatus status, const gchar *msg);

G_END_DECLS

#endif /* GST_MFX_UTILS_VAAPI_H */
