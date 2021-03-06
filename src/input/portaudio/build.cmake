# The project default
option(PORTAUDIO "PORTAUDIO" ON)

if(PORTAUDIO)
	pkg_check_modules(PORTAUDIO QUIET portaudio-2.0)
	if(PORTAUDIO_FOUND)
		add_definitions(-DPORTAUDIO)
		add_library(in_portaudio SHARED "${XAVA_MODULE_DIR}/main.c"
										"${GLOBAL_FUNCTION_SOURCES}")
		target_link_libraries(in_portaudio "${PORTAUDIO_LIBRARIES}" iniparser)
		target_include_directories(in_portaudio PRIVATE "${PORTAUDIO_INCLUDE_DIRS}")
		target_link_directories(in_portaudio PRIVATE "${PORTAUDIO_LIBRARY_DIRS}")
		set_target_properties(in_portaudio PROPERTIES PREFIX "")
		install(TARGETS in_portaudio DESTINATION lib/xava)
	else()
		message(STATUS "portaudio library not found")
	endif()
endif()
