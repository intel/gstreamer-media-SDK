#include "video-utils.h"

/* Check VA status for success or print out an error */
gboolean
vaapi_check_status (VAStatus status, const gchar * msg)
{
    if (status != VA_STATUS_SUCCESS) {
        GST_DEBUG ("%s: %s", msg, vaErrorStr (status));
        return FALSE;
    }
    return TRUE;
}
