# Default filter, is well..... default
option(FILTER_DEFAULT "FILTER_DEFAULT" ON)

# fftw3
pkg_check_modules(FFTW3 REQUIRED fftw3f)
list(APPEND INCLUDE_DIRS "${FFTW3_INCLUDE_DIRS}")
list(APPEND LINK_DIRS "${FFTW3_LIBRARY_DIRS}")

if(FILTER_DEFAULT)
	message(STATUS "Default filter enabled!")
	add_library(filter_default SHARED "${XAVA_MODULE_DIR}/main.c"
										"${GLOBAL_FUNCTION_SOURCES}")
	target_link_libraries(filter_default xava-shared "${FFTW3_LIBRARIES}" iniparser)
	set_target_properties(filter_default PROPERTIES PREFIX "")
	install(TARGETS filter_default DESTINATION lib/xava)
endif()

