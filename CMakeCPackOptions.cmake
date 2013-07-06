# Determine current architecture
macro(dpkg_arch VAR_NAME)
	find_program(DPKG_PROGRAM dpkg DOC "dpkg program of Debian-based systems")
	if (DPKG_PROGRAM) 
	  execute_process(
	    COMMAND ${DPKG_PROGRAM} --print-architecture
	    OUTPUT_VARIABLE ${VAR_NAME}
	    OUTPUT_STRIP_TRAILING_WHITESPACE
	  )
	endif(DPKG_PROGRAM)
endmacro(dpkg_arch)


# CPack configuration
set(CPACK_PACKAGE_NAME "deadbeef-plugin-vk")
set(CPACK_PACKAGE_VENDOR "https://github.com/scorpp/db-vk")
set(CPACK_PACKAGE_VERSION "0.1-12")
set(CPACK_PACKAGE_CONTACT "keryascorpio@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Deadbeef plugin for VKontakte")

# DEB package config
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kirill Malyshev <keryascorpio@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/scorpp/db-vk")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgtk2.0-0, libcurl3, deadbeef, libjson-glib-1.0-0")
if (${CPACK_GENERATOR} STREQUAL "DEB")
	set(CPACK_SET_DESTDIR true)
	set(CPACK_INSTALL_PREFIX /opt/deadbeef)
	
	dpkg_arch(CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
	if (CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
		set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE})
	else (CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
		set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CMAKE_SYSTEM_NAME})
	endif (CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
endif (${CPACK_GENERATOR} STREQUAL "DEB")
