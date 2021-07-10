# Project default
option(PIPEWIRE "PIPEWIRE" ON)

# libpipewire-0.3
if(PIPEWIRE)
	pkg_check_modules(PIPEWIRE QUIET libspa-0.2 libpipewire-0.3)
	if(PIPEWIRE_FOUND)
		add_definitions(-DPIPEWIRE)
		add_library(in_pipewire SHARED "${XAVA_MODULE_DIR}/main.c"
									"${GLOBAL_FUNCTION_SOURCES}")
		target_link_libraries(in_pipewire xava-shared "${PIPEWIRE_LIBRARIES}" iniparser)
		target_include_directories(in_pipewire PRIVATE "${PIPEWIRE_INCLUDE_DIRS}")
		target_link_directories(in_pipewire PRIVATE "${PIPEWIRE_LIBRARY_DIRS}")
		set_target_properties(in_pipewire PROPERTIES PREFIX "")
		install(TARGETS in_pipewire DESTINATION lib/xava)
	else()
		message(STATUS "pipewire library not found")
	endif()
endif()

