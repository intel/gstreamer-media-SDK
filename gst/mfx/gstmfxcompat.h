#ifndef GST_COMPAT_H
#define GST_COMPAT_H

#include "sysdeps.h"

#if !GST_CHECK_VERSION (1,5,0)
static inline GstBuffer *
gst_buffer_copy_deep (const GstBuffer * buffer)
{
	  GstBuffer *copy;

	    g_return_val_if_fail (buffer != NULL, NULL);

	      copy = gst_buffer_new ();

	        if (!gst_buffer_copy_into (copy, (GstBuffer *) buffer,
					      GST_BUFFER_COPY_ALL | GST_BUFFER_COPY_DEEP, 0, -1))
			    gst_buffer_replace (&copy, NULL);

#if GST_CHECK_VERSION (1,4,0)
		  if (copy)
			      GST_BUFFER_FLAG_UNSET (copy, GST_BUFFER_FLAG_TAG_MEMORY);
#endif

		    return copy;
}
#endif

#endif /* GST_COMPAT_H */
