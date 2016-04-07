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

mfxStatus
vaapi_to_mfx_status(VAStatus va_sts)
{
	mfxStatus mfx_sts = MFX_ERR_NONE;

	switch (va_sts) {
	case VA_STATUS_SUCCESS:
		mfx_sts = MFX_ERR_NONE;
		break;
	case VA_STATUS_ERROR_ALLOCATION_FAILED:
		mfx_sts = MFX_ERR_MEMORY_ALLOC;
		break;
	case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
	case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
	case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
	case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
	case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
	case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
	case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
		mfx_sts = MFX_ERR_UNSUPPORTED;
		break;
	case VA_STATUS_ERROR_INVALID_DISPLAY:
	case VA_STATUS_ERROR_INVALID_CONFIG:
	case VA_STATUS_ERROR_INVALID_CONTEXT:
	case VA_STATUS_ERROR_INVALID_SURFACE:
	case VA_STATUS_ERROR_INVALID_BUFFER:
	case VA_STATUS_ERROR_INVALID_IMAGE:
	case VA_STATUS_ERROR_INVALID_SUBPICTURE:
		mfx_sts = MFX_ERR_NOT_INITIALIZED;
		break;
	case VA_STATUS_ERROR_INVALID_PARAMETER:
		mfx_sts = MFX_ERR_INVALID_VIDEO_PARAM;
	default:
		mfx_sts = MFX_ERR_UNKNOWN;
		break;
	}
	return mfx_sts;
}
