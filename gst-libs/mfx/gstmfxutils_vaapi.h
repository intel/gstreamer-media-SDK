#ifndef GST_MFX_UTILS_VAAPI_H
#define GST_MFX_UTILS_VAAPI_H

//#include <gst/gstbuffer.h>
#include "gstmfxobject.h"
#include "gstmfxdisplay.h"
#include "video-utils.h"

G_BEGIN_DECLS

#define VAAPI_IMAGE(obj) \
    ((VaapiImage *)(obj))

/**
 * VAAPI_IMAGE_FORMAT:
 * @image: a #VaapiImage
 *
 * Macro that evaluates to the #GstVideoFormat of @image.
 */
#define VAAPI_IMAGE_FORMAT(image)   vaapi_image_get_format(image)

/**
 * VAAPI_IMAGE_WIDTH:
 * @image: a #VaapiImage
 *
 * Macro that evaluates to the width of @image.
 */
#define VAAPI_IMAGE_WIDTH(image)    vaapi_image_get_width(image)

/**
 * VAAPI_IMAGE_HEIGHT:
 * @image: a #VaapiImage
 *
 * Macro that evaluates to the height of @image.
 */
#define VAAPI_IMAGE_HEIGHT(image)   vaapi_image_get_height(image)

typedef struct _VaapiImage      VaapiImage;
typedef struct _VaapiImageClass VaapiImageClass;

VaapiImage *
vaapi_image_new(
	GstMfxDisplay   *display,
    guint           width,
    guint           height
);

VaapiImage *
vaapi_image_new_with_image(GstMfxDisplay *display, VAImage *va_image);

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
vaapi_image_is_mapped(VaapiImage *image);

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


G_END_DECLS

#endif /* GST_MFX_UTILS_VAAPI_H */
