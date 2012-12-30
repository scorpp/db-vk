project(DB_VK)
cmake_minimum_required(VERSION 2.6)

option(USE_GTK2 "Use GTK2 instead of default GTK3" OFF)

find_package(PkgConfig REQUIRED)

if (USE_GTK2)
	pkg_check_modules(GTK REQUIRED gtk+-2.0)
else (USE_GTK2)
	pkg_check_modules(GTK REQUIRED gtk+-3.0)
endif (USE_GTK2)

include_directories(${GTK_INCLUDE_DIRS})
link_directories(${GTK_LIBRARY_DIRS})
add_definitions(${GTK_CFLAGS_OTHER})


pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
include_directories(${JSON_GLIB_INCLUDE_DIRS})
link_directories(${JSON_GLIB_LIBRARY_DIRS})
add_definitions(${JSON_GLIB_CFLAGS_OTHER})

find_package(CURL REQUIRED)

include_directories(${DB_VK_SOURCE_DIR})

set(CMAKE_C_FLAGS "-g -Wall")

add_library(vkontakte SHARED vkontakte.c vk-api.c ui.c util.c)
target_link_libraries(vkontakte ${GTK_LIBRARIES} ${CURL_LIBRARIES} ${JSON_GLIB_LIBRARIES})
set_target_properties(vkontakte PROPERTIES PREFIX "")


# Configure packaging
set(CPACK_PROJECT_CONFIG_FILE ${PROJECT_SOURCE_DIR}/CMakeCPackOptions.cmake) 

install(TARGETS vkontakte DESTINATION "lib/deadbeef")

include(CPack)
