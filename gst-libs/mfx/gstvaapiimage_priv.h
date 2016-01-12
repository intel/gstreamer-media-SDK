#ifndef GST_VAAPI_IMAGE_PRIV_H
#define GST_VAAPI_IMAGE_PRIV_H

#include "gstmfxobject_priv.h"
#include "gstvaapiimage.h"

#include <va/va.h>

G_BEGIN_DECLS

typedef struct _GstVaapiImageClass GstVaapiImageClass;

/**
 * GstVaapiImage:
 *
 * A VA image wrapper
 */
struct _GstVaapiImage {
    /*< private >*/
    GstMfxObject		parent_instance;

    GstVideoFormat      internal_format;
    guchar             *image_data;
    guint               width;
    guint               height;
    VAImage             image;
};

/**
* GstVaapiImageClass:
*
* A VA image wrapper class
*/
struct _GstVaapiImageClass {
	/*< private >*/
	GstMfxObjectClass parent_class;
};

/**
 * GST_VAAPI_IMAGE_FORMAT:
 * @image: a #GstVaapiImage
 *
 * Macro that evaluates to the #GstVideoFormat of @image.
 */
#undef  GST_VAAPI_IMAGE_FORMAT
#define GST_VAAPI_IMAGE_FORMAT(image) \
    (GST_VAAPI_IMAGE(image)->internal_format)

/**
 * GST_VAAPI_IMAGE_WIDTH:
 * @image: a #GstVaapiImage
 *
 * Macro that evaluates to the width of @image.
 */
#undef  GST_VAAPI_IMAGE_WIDTH
#define GST_VAAPI_IMAGE_WIDTH(image) \
    (GST_VAAPI_IMAGE(image)->width)

/**
 * GST_VAAPI_IMAGE_HEIGHT:
 * @image: a #GstVaapiImage
 *
 * Macro that evaluates to the height of @image.
 */
#undef  GST_VAAPI_IMAGE_HEIGHT
#define GST_VAAPI_IMAGE_HEIGHT(image) \
    (GST_VAAPI_IMAGE(image)->height)


G_END_DECLS

#endif /* GST_VAAPI_IMAGE_PRIV_H */
