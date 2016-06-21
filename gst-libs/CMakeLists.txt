file(GLOB SOURCE  "mfx/*.c")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${GSTREAMER_LIBDIR}/gstreamer-1.0")
endif()


if (HAVE_WAYLAND_RENDERER)
	file(GLOB WAYLAND_SOURCE "mfx/wayland/*.c")
	include_directories( mfx/wayland )
endif()
if (HAVE_X11_RENDERER)
	file(GLOB X11_SOURCE "mfx/x11/*.c")
	include_directories ( mfx/x11 )
endif()
if (HAVE_EGL_RENDERER)
	file(GLOB EGL_RENDERER "mfx/egl/*.c")
	include_directories ( mfx/egl )
endif()

file(GLOB COMMON_UTILS "mfx/common/*.c")
include_directories ( mfx/common )

add_library(gstmfxhelper SHARED ${SOURCE} ${WAYLAND_SOURCE} ${X11_SOURCE} ${EGL_RENDERER} ${COMMON_UTILS})
target_link_libraries(gstmfxhelper
	${GLIB-2.0_LIBRARIES}
	${GOBJECT-2.0_LIBRARIES}
	${GSTREAMER_LIBRARIES}
	${GSTREAMER_VIDEO_LIBRARIES}
	${LIBVA_LIBRARIES}
	${LIBVA_DRM_LIBRARIES}
	${LIBDRM_LIBRARIES}
	${LIBDRM_INTEL_LIBRARIES}
	${LIBUDEV_LIBRARIES}
	${SINK_LIBRARIES}
	libmfx)

install (TARGETS gstmfxhelper 
	LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX} 
	RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
	)

