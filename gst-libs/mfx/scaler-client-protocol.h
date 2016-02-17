/* 
 * Copyright © 2013-2014 Collabora, Ltd.
 * 
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#ifndef SCALER_CLIENT_PROTOCOL_H
#define SCALER_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_scaler;
struct wl_viewport;

extern const struct wl_interface wl_scaler_interface;
extern const struct wl_interface wl_viewport_interface;

#ifndef WL_SCALER_ERROR_ENUM
#define WL_SCALER_ERROR_ENUM
enum wl_scaler_error {
	WL_SCALER_ERROR_VIEWPORT_EXISTS = 0,
};
#endif /* WL_SCALER_ERROR_ENUM */

#define WL_SCALER_DESTROY	0
#define WL_SCALER_GET_VIEWPORT	1

static inline void
wl_scaler_set_user_data(struct wl_scaler *wl_scaler, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_scaler, user_data);
}

static inline void *
wl_scaler_get_user_data(struct wl_scaler *wl_scaler)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_scaler);
}

static inline void
wl_scaler_destroy(struct wl_scaler *wl_scaler)
{
	wl_proxy_marshal((struct wl_proxy *) wl_scaler,
			 WL_SCALER_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_scaler);
}

static inline struct wl_viewport *
wl_scaler_get_viewport(struct wl_scaler *wl_scaler, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_scaler,
			 WL_SCALER_GET_VIEWPORT, &wl_viewport_interface, NULL, surface);

	return (struct wl_viewport *) id;
}

#ifndef WL_VIEWPORT_ERROR_ENUM
#define WL_VIEWPORT_ERROR_ENUM
enum wl_viewport_error {
	WL_VIEWPORT_ERROR_BAD_VALUE = 0,
};
#endif /* WL_VIEWPORT_ERROR_ENUM */

#define WL_VIEWPORT_DESTROY	0
#define WL_VIEWPORT_SET	1
#define WL_VIEWPORT_SET_SOURCE	2
#define WL_VIEWPORT_SET_DESTINATION	3

static inline void
wl_viewport_set_user_data(struct wl_viewport *wl_viewport, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_viewport, user_data);
}

static inline void *
wl_viewport_get_user_data(struct wl_viewport *wl_viewport)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_viewport);
}

static inline void
wl_viewport_destroy(struct wl_viewport *wl_viewport)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_viewport);
}

static inline void
wl_viewport_set(struct wl_viewport *wl_viewport, wl_fixed_t src_x, wl_fixed_t src_y, wl_fixed_t src_width, wl_fixed_t src_height, int32_t dst_width, int32_t dst_height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_SET, src_x, src_y, src_width, src_height, dst_width, dst_height);
}

static inline void
wl_viewport_set_source(struct wl_viewport *wl_viewport, wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_SET_SOURCE, x, y, width, height);
}

static inline void
wl_viewport_set_destination(struct wl_viewport *wl_viewport, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_SET_DESTINATION, width, height);
}

#ifdef  __cplusplus
}
#endif

#endif
