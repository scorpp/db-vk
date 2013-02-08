project(DB_VK)
cmake_minimum_required(VERSION 2.6)

# Build both by default
option(WITH_GTK2 "Build GTK2 version" ON)
option(WITH_GTK3 "Build GTK3 version" ON)

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
include_directories(${JSON_GLIB_INCLUDE_DIRS})
link_directories(${JSON_GLIB_LIBRARY_DIRS})
add_definitions(${JSON_GLIB_CFLAGS_OTHER})

include_directories(${DB_VK_SOURCE_DIR})

set(CMAKE_C_FLAGS "-g -Wall")

if (WITH_GTK2)
	pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
	add_library(vkontakte_gtk2 SHARED vkontakte.c vk-api.c ui.c util.c)
	target_link_libraries(vkontakte_gtk2 ${GTK2_LIBRARIES} ${CURL_LIBRARIES} ${JSON_GLIB_LIBRARIES})
	
	get_target_property(VK_GTK2_INCLUDE_DIRS vkontakte_gtk2 INCLUDE_DIRECTORIES)
	set_target_properties(vkontakte_gtk2 PROPERTIES INCLUDE_DIRECTORIES "${VK_GTK2_INCLUDE_DIRS};${GTK2_INCLUDE_DIRS}")

        link_directories(${GTK2_LIBRARY_DIRS})
	
	set_target_properties(vkontakte_gtk2 PROPERTIES PREFIX "")
	install(TARGETS vkontakte_gtk2 DESTINATION "lib${LIB_SUFFIX}/deadbeef")
endif ()

if (WITH_GTK3)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	add_library(vkontakte_gtk3 SHARED vkontakte.c vk-api.c ui.c util.c)
	target_link_libraries(vkontakte_gtk3 ${GTK3_LIBRARIES} ${CURL_LIBRARIES} ${JSON_GLIB_LIBRARIES})
        
        get_target_property(VK_GTK3_INCLUDE_DIRS vkontakte_gtk3 INCLUDE_DIRECTORIES)
	set_target_properties(vkontakte_gtk3 PROPERTIES INCLUDE_DIRECTORIES "${VK_GTK3_INCLUDE_DIRS};${GTK3_INCLUDE_DIRS}") 

	link_directories(${GTK2_LIBRARY_DIRS})
	
	set_target_properties(vkontakte_gtk3 PROPERTIES PREFIX "")
	install(TARGETS vkontakte_gtk3 DESTINATION "lib${LIB_SUFFIX}/deadbeef")
endif ()


# Configure packaging
set(CPACK_PROJECT_CONFIG_FILE ${PROJECT_SOURCE_DIR}/CMakeCPackOptions.cmake) 
include(CPack)
