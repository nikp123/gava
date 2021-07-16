project(xava C CXX)
cmake_minimum_required(VERSION 3.13)

# Fix pkg-config for cross-builds (such as MinGW on ArchLinux)
if(CMAKE_FIND_ROOT_PATH)
    set(CMAKE_SYSROOT "${CMAKE_FIND_ROOT_PATH}")

    set(ENV{PKG_CONFIG_DIR} "")
    set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/lib/pkgconfig:${CMAKE_SYSROOT}/share/pkgconfig")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
endif()

# Debug is default
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

# Define XAVA version
set(xava_VERSION_MAJOR 0)
set(xava_VERSION_MINOR 7)
set(xava_VERSION_PATCH 0)
set(xava_VERSION_TWEAK 0)
set(xava_VERSION "${xava_VERSION_MAJOR}.${xava_VERSION_MINOR}.${xava_VERSION_PATCH}.${xava_VERSION_TWEAK}")

# Definitions
add_definitions(-DPACKAGE="xava" -DVERSION="${xava_VERSION}")
add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")

# Windows-y things
if(MINGW)
	set(CMAKE_FIND_LIBRARY_SUFFIXES .dll ${CMAKE_FIND_LIBRARY_SUFFIXES})
	add_definitions(-DXAVA_DEFAULT_INPUT="wasapi" -DXAVA_DEFAULT_OUTPUT="win")

	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		message(STATUS "Since release build, console is being disabled")
		SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} -mwindows")

		# Prepare NSI file for compilation
		configure_file("assets/windows/xava.nsi.template" "xava.nsi" NEWLINE_STYLE CRLF)
	endif()
else()
	# This hack is a of courtesy of: https://stackoverflow.com/a/40947954/3832385
	string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
	add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")
endif()


# Set appropriate C flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -fvisibility='internal' ${BUILD_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS "-Wall -fvisibility='internal' ${BUILD_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")


find_package(PkgConfig REQUIRED)

# iniparser
find_library(INIPARSER iniparser HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
if(NOT INIPARSER)
	message(STATUS "iniparser not found on system, building from source.")
	if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/.git)
		execute_process(COMMAND git submodule update --init)
		add_library(iniparser
				lib/iniparser/src/dictionary.c
				lib/iniparser/src/iniparser.c)
		add_definitions(-DINIPARSER)
	else()
		message(FATAL_ERROR "Cannot build iniparser from source since the project \
is not being built inside a git repository.\nIf you want iniparser \
to be built automatically, clone this repository instead of downloading it.")
	endif()
else()
	# certain distros like ubuntu put iniparser in a subdirectory "iniparser"
	# this is just a non-destructive way to accomidate that case

	find_file(INIPARSER_INCLUDE_FILE iniparser/iniparser.h ${CMAKE_SYSTEM_INCLUDE_PATH})
	if(NOT ${INIPARSER_INCLUDE_FILE} STREQUAL "INIPARSER_INCLUDE_FILE-NOTFOUND")
		string(REGEX REPLACE "iniparser.h" "" INIPARSER_INCLUDE_DIR ${INIPARSER_INCLUDE_FILE})
		list(APPEND INCLUDE_DIRS ${INIPARSER_INCLUDE_DIR})
	endif()
endif()

if(DEFINE_LEGACYINIPARSER AND INIPARSER)
	add_definitions(-DLEGACYINIPARSER)
endif()

# math lib
find_library(MATH_LIB m HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
if(NOT MATH_LIB)
	message(FATAL_ERROR "math library not found")
endif()

# pthread
find_library(THREAD_LIB pthread HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
if(NOT THREAD_LIB)
	message(FATAL_ERROR "pthread library not found")
endif()

# Optional features
set(ADDITIONAL_SOURCES "")
set(ADDITIONAL_LIBRARIES "")

if((UNIX AND NOT APPLE) OR (MSYS OR MINGW OR MSVC))
	list(APPEND ADDITIONAL_SOURCES "src/misc/inode_watcher.c")
endif()

# Shitty workaround that doesnt work for BSD, Minix or anything with any amount of POSIX compatibility
# Blame: https://stackoverflow.com/questions/9160335/os-specific-instructions-in-cmake-how-to
# Also CMAKE_SYSTEM_NAME doesn't work, so I've tried my options. 
# Please fix this whoever has time and reads this.
if(UNIX AND NOT APPLE)
	# Add proper version info to the desktop shortcut file
	configure_file("assets/linux/xava.desktop.template" "xava.desktop" NEWLINE_STYLE UNIX)
endif()

# Runtime library load
if(UNIX)
	list(APPEND ADDITIONAL_LIBRARIES "dl")
endif()

# Build filter modules
file(GLOB filters "src/filters/*/build.cmake" )
foreach(dir ${filters})
	get_filename_component(XAVA_MODULE_DIR ${dir} DIRECTORY)
	include(${dir})
endforeach()

# Build output modules
file(GLOB outputs "src/output/*/build.cmake" )
foreach(dir ${outputs})
	get_filename_component(XAVA_MODULE_DIR ${dir} DIRECTORY)
	include(${dir})
endforeach()

# Build input modules
file(GLOB inputs "src/input/*/build.cmake" )
foreach(dir ${inputs})
	get_filename_component(XAVA_MODULE_DIR ${dir} DIRECTORY)
	include(${dir})
endforeach()

# Build xava shared library
add_library(xava-shared SHARED
	src/log.c
	src/shared.c)

# Build executable
add_executable(
	xava
	${ADDITIONAL_SOURCES}
	src/module.c
	src/config.c
	src/xava.c
)

include_directories(${INCLUDE_DIRS})
link_directories(${LINK_DIRS})

# Link executable
target_link_libraries(xava xava-shared ${ADDITIONAL_LIBRARIES} m pthread iniparser)

# Install
install (TARGETS xava DESTINATION bin)
install (TARGETS xava-shared DESTINATION lib)
install (FILES LICENSE DESTINATION share/licenses/xava)
install (FILES example_files/config RENAME config.example DESTINATION share/xava)
install (FILES example_files/egl/shaders/pre.vert RENAME pre.vert.example DESTINATION share/xava/egl/shaders)
install (FILES example_files/egl/shaders/pre.frag RENAME pre.frag.example DESTINATION share/xava/egl/shaders)
install (FILES example_files/egl/shaders/post.vert RENAME post.vert.example DESTINATION share/xava/egl/shaders)
install (FILES example_files/egl/shaders/post.frag RENAME post.frag.example DESTINATION share/xava/egl/shaders)
if(UNIX AND NOT APPLE)
    install (FILES ${CMAKE_BINARY_DIR}/xava.desktop DESTINATION share/applications)
    install (FILES assets/linux/xava.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()
