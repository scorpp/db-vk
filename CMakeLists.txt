project(DB_VK)
cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
include_directories(${JSON_GLIB_INCLUDE_DIRS})
link_directories(${JSON_GLIB_LIBRARY_DIRS})
add_definitions(${JSON_GLIB_CFLAGS_OTHER})

find_package(CURL REQUIRED)

include_directories(${DB_VK_SOURCE_DIR})

set(CMAKE_C_FLAGS "-g -Wall")

add_library(vkontakte SHARED vkontakte.c vk-api.c ui.c util.c)
target_link_libraries(vkontakte ${GTK3_LIBRARIES} ${CURL_LIBRARIES} ${JSON_GLIB_LIBRARIES})
set_target_properties(vkontakte PROPERTIES PREFIX "")
