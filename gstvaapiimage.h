#ifndef GST_VAAPI_IMAGE_H
#define GST_VAAPI_IMAGE_H

//#include <gst/gstbuffer.h>
#include "gstmfxobject.h"
#include "gstmfxdisplay.h"
#include "video-utils.h"

G_BEGIN_DECLS

#define GST_VAAPI_IMAGE(obj) \
    ((GstVaapiImage *)(obj))

/**
 * GST_VAAPI_IMAGE_FORMAT:
 * @image: a #GstVaapiImage
 *
 * Macro that evaluates to the #GstVideoFormat of @image.
 */
#define GST_VAAPI_IMAGE_FORMAT(image)   gst_vaapi_image_get_format(image)

/**
 * GST_VAAPI_IMAGE_WIDTH:
 * @image: a #GstVaapiImage
 *
 * Macro that evaluates to the width of @image.
 */
#define GST_VAAPI_IMAGE_WIDTH(image)    gst_vaapi_image_get_width(image)

/**
 * GST_VAAPI_IMAGE_HEIGHT:
 * @image: a #GstVaapiImage
 *
 * Macro that evaluates to the height of @image.
 */
#define GST_VAAPI_IMAGE_HEIGHT(image)   gst_vaapi_image_get_height(image)

typedef struct _GstVaapiImage GstVaapiImage;

GstVaapiImage *
gst_vaapi_image_new(
	GstMfxDisplay   *display,
    guint           width,
    guint           height
);

GstVaapiImage *
gst_vaapi_image_new_with_image(GstMfxDisplay *display, VAImage *va_image);

VAImageID
gst_vaapi_image_get_id(GstVaapiImage *image);

gboolean
gst_vaapi_image_get_image(GstVaapiImage *image, VAImage *va_image);

GstVideoFormat
gst_vaapi_image_get_format(GstVaapiImage *image);

guint
gst_vaapi_image_get_width(GstVaapiImage *image);

guint
gst_vaapi_image_get_height(GstVaapiImage *image);

void
gst_vaapi_image_get_size(GstVaapiImage *image, guint *pwidth, guint *pheight);

gboolean
gst_vaapi_image_is_mapped(GstVaapiImage *image);

gboolean
gst_vaapi_image_map(GstVaapiImage *image);

gboolean
gst_vaapi_image_unmap(GstVaapiImage *image);

guint
gst_vaapi_image_get_plane_count(GstVaapiImage *image);

guchar *
gst_vaapi_image_get_plane(GstVaapiImage *image, guint plane);

guint
gst_vaapi_image_get_pitch(GstVaapiImage *image, guint plane);

guint
gst_vaapi_image_get_data_size(GstVaapiImage *image);


G_END_DECLS

#endif /* GST_VAAPI_IMAGE_H */
