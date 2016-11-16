project(DB_VK)
cmake_minimum_required(VERSION 2.6)

# Build both by default
option(WITH_GTK2 "Build GTK2 version")
option(WITH_GTK3 "Build GTK3 version")

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(JANSSON REQUIRED jansson)
include_directories(${JANSSON_INCLUDE_DIRS})
link_directories(${JANSSON_LIBRARY_DIRS})
add_compile_options(${JANSSON_CFLAGS})

include_directories(${DB_VK_SOURCE_DIR})

set(CMAKE_C_FLAGS "-g -Wall")

file(GLOB VK_PLUGIN_SRC "src/*.h" "src/*.c")

# Older versions of cmake does not support modifying INCLUDE_DIRECTORIES for a target
# by means of SET_TARGET_PROPERTIES command
if (CMAKE_MAJOR_VERSION EQUAL "2" 
		AND (CMAKE_MINOR_VERSION LESS "8" OR CMAKE_MINOR_VERSION EQUAL "8")
		AND CMAKE_PATCH_VERSION LESS "10")
	set(CMAKE_PRIOR_TO_2_8_10 "ON")
endif()
# With this old version we can't build two versions at the same time :(
if (CMAKE_PRIOR_TO_2_8_10 AND WITH_GTK2 AND WITH_GTK3)
	message(FATAL_ERROR "Cannot build both GTK2 & GTK3 libs at once on cmake ${CMAKE_VERSION}. Build with either version of GTK separately (e.g. -DWITH_GTK2=ON -DWITH_GTK3=OFF) or upgrade cmake to at least 2.8.10")
endif()


pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
if (WITH_GTK2 AND NOT GTK2_FOUND)
	message(FATAL_ERROR "GTK2 development files are not installed")
endif ()

if (GTK2_FOUND)
	add_library(vkontakte_gtk2 SHARED ${VK_PLUGIN_SRC})
	target_link_libraries(vkontakte_gtk2 ${GTK2_LIBRARIES} ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
	
	if (CMAKE_PRIOR_TO_2_8_10)
		include_directories(${GTK2_INCLUDE_DIRS})
	else()
		get_target_property(VK_GTK2_INCLUDE_DIRS vkontakte_gtk2 INCLUDE_DIRECTORIES)
		set_target_properties(vkontakte_gtk2 PROPERTIES INCLUDE_DIRECTORIES "${VK_GTK2_INCLUDE_DIRS};${GTK2_INCLUDE_DIRS}")
	endif()

    link_directories(${GTK2_LIBRARY_DIRS})
	
	set_target_properties(vkontakte_gtk2 PROPERTIES PREFIX "")
	install(TARGETS vkontakte_gtk2 DESTINATION "lib${LIB_SUFFIX}/deadbeef")
endif ()

pkg_check_modules(GTK3 gtk+-3.0)
if (WITH_GTK3 AND NOT GTK3_FOUND)
    message(FATAL_ERROR "GTK3 development files are not installed")
endif ()

if (GTK3_FOUND)
	add_library(vkontakte_gtk3 SHARED ${VK_PLUGIN_SRC})
	target_link_libraries(vkontakte_gtk3 ${GTK3_LIBRARIES} ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
        
    if (CMAKE_PRIOR_TO_2_8_10)
		include_directories(${GTK3_INCLUDE_DIRS})
	else()
	    get_target_property(VK_GTK3_INCLUDE_DIRS vkontakte_gtk3 INCLUDE_DIRECTORIES)
		set_target_properties(vkontakte_gtk3 PROPERTIES INCLUDE_DIRECTORIES "${VK_GTK3_INCLUDE_DIRS};${GTK3_INCLUDE_DIRS}")
	endif() 

	link_directories(${GTK3_LIBRARY_DIRS})
	
	set_target_properties(vkontakte_gtk3 PROPERTIES PREFIX "")
	install(TARGETS vkontakte_gtk3 DESTINATION "lib${LIB_SUFFIX}/deadbeef")
endif ()

if (!GTK2_FOUND AND !GTK3_FOUND)
	message(FATAL_ERROR "Either GTK2 or GTK3 required")
endif ()

# Configure packaging
set(CPACK_PROJECT_CONFIG_FILE ${PROJECT_SOURCE_DIR}/CMakeCPackOptions.cmake) 
include(CPack)
