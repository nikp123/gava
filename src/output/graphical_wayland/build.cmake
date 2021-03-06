# Wayland protocols default directory
set(WL_PROT_DIR "/usr/share/wayland-protocols")

# Project default
option(WAYLAND "WAYLAND" ON)

# Wayland
if(WAYLAND)
	pkg_check_modules(WAYLAND QUIET egl wayland-client wayland-egl)
	if(WAYLAND_FOUND)
		set(_CFLAG_SYMBOLS_HIDE "-fvisibility=hidden")

		# Hacky way to deal with wayland-scanner (but will be used for nw)
		execute_process(COMMAND wayland-scanner client-header 
			"${WL_PROT_DIR}/stable/xdg-shell/xdg-shell.xml"
			"${XAVA_MODULE_DIR}/gen/xdg-shell-client-protocol.h")
		execute_process(COMMAND wayland-scanner private-code 
			"${WL_PROT_DIR}/stable/xdg-shell/xdg-shell.xml"
			"${XAVA_MODULE_DIR}/gen/xdg-shell-client-protocol.c")
		execute_process(COMMAND wayland-scanner client-header
			"${WL_PROT_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml"
			"${XAVA_MODULE_DIR}/gen/xdg-output-unstable-v1-client-protocol.h")
		execute_process(COMMAND wayland-scanner private-code
			"${WL_PROT_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml"
			"${XAVA_MODULE_DIR}/gen/xdg-output-unstable-v1-client-protocol.c")

		execute_process(COMMAND wayland-scanner client-header
			"${XAVA_MODULE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml"
			"${XAVA_MODULE_DIR}/gen/wlr-layer-shell-unstable-v1-client-protocol.h")
		execute_process(COMMAND wayland-scanner private-code
			"${XAVA_MODULE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml"
			"${XAVA_MODULE_DIR}/gen/wlr-layer-shell-unstable-v1-client-protocol.c")
		execute_process(COMMAND wayland-scanner client-header
			"${XAVA_MODULE_DIR}/protocols/wlr-output-management-unstable-v1.xml"
			"${XAVA_MODULE_DIR}/gen/wlr-output-managment-unstable-v1.h")
		execute_process(COMMAND wayland-scanner private-code
			"${XAVA_MODULE_DIR}/protocols/wlr-output-management-unstable-v1.xml"
			"${XAVA_MODULE_DIR}/gen/wlr-output-managment-unstable-v1.c")

		add_library(out_wayland SHARED
			"${XAVA_MODULE_DIR}/main.c"
			"${XAVA_MODULE_DIR}/render.c"
			"${XAVA_MODULE_DIR}/wl_output.c"
			"${XAVA_MODULE_DIR}/registry.c"
			"${XAVA_MODULE_DIR}/xdg.c"
			"${XAVA_MODULE_DIR}/zwlr.c"
			"src/output/graphical.c"
			"${XAVA_MODULE_DIR}/gen/xdg-shell-client-protocol.c"
			"${XAVA_MODULE_DIR}/gen/xdg-output-unstable-v1-client-protocol.c"
			"${XAVA_MODULE_DIR}/gen/wlr-output-managment-unstable-v1.c"
			"${XAVA_MODULE_DIR}/gen/wlr-layer-shell-unstable-v1-client-protocol.c"
			"${GLOBAL_FUNCTION_SOURCES}")
		target_link_libraries(out_wayland "${WAYLAND_LIBRARIES}" iniparser)
		target_include_directories(out_wayland PRIVATE "${WAYLAND_INCLUDE_DIRS}")
		target_link_directories(out_wayland PRIVATE "${WAYLAND_LIBRARY_DIRS} -lrt")
		set_target_properties(out_wayland PROPERTIES PREFIX "")
		install(TARGETS out_wayland DESTINATION lib/xava)
		add_definitions(-DWAYLAND)
	else()
		message(STATUS "Wayland libraries not found")
	endif()
endif()
