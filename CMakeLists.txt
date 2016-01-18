project(openelp C)

cmake_minimum_required(VERSION 2.7)

#
# Installation directories
#

set(SHARE_INSTALL_PREFIX "share" CACHE PATH
  "Installation prefix for all application data"
  )
set(BIN_INSTALL_DIR "bin" CACHE PATH
  "Installation directory for all binaries"
  )
set(INCLUDE_INSTALL_DIR "include" CACHE PATH
  "Installation directory for all header files"
  )
set(LIB_SUFFIX "" CACHE STRING
  "Suffix appended for the library installation directory"
  )
set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}" CACHE PATH
  "Installation directory for all shared libraries"
  )
if(WIN32)
  set(SYSCONF_INSTALL_DIR "./" CACHE PATH
    "Installation directory for all configuration files"
    )
else()
  set(SYSCONF_INSTALL_DIR "/etc" CACHE PATH
    "Installation directory for all configuration files"
    )
endif()
set(MAN_INSTALL_DIR "${SHARE_INSTALL_PREFIX}/man" CACHE PATH
  "Installation directory for all manuals"
  )
set(SYSTEMD_SERVICES_INSTALL_DIR "lib/systemd/system" CACHE PATH
  "Installation directory for systemd services"
  )

#
# Configuration Variables
#

set(BUILD_SHARED_LIBS ON CACHE BOOL
  "Indicates that any libraries which are produced should be shared (.dll/.so)"
  )
set(OPENELP_BUNDLE_PCRE FALSE CACHE BOOL
  "Download and compile PCRE2 instead of using the system's libpcre2-32"
  )
set(OPENELP_DOC_INTERNAL FALSE CACHE BOOL
  "Include internal code when documenting API"
  )
set(OPENELP_USE_OPENSSL FALSE CACHE BOOL
  "Use OpenSSL for MD5 computation instead of bundled md5.c"
  )
if(WIN32)
  set(OPENELP_USE_EVENTLOG TRUE CACHE BOOL
    "Enable support for logging to the Windows Event Log"
    )
  set(OPENELP_USE_SYSLOG FALSE CACHE BOOL
    "Enable support for logging to SYSLOG"
    )
  set(OPENELP_CONFIG_HINT "../ELProxy.conf" CACHE PATH
    "Hint path when searching for the proxy configuration file at runtime"
    )
  set(OPENELP_DOC_HTMLHELP TRUE CACHE BOOL
    "Enable generation of Windows CHM help"
    )
else()
  set(OPENELP_USE_EVENTLOG FALSE CACHE BOOL
    "Enable support for logging to the Windows Event Log"
    )
  set(OPENELP_USE_SYSLOG TRUE CACHE BOOL
    "Enable support for logging to SYSLOG"
    )
  set(OPENELP_CONFIG_HINT "/etc/ELProxy.conf" CACHE PATH
    "Hint path when searching for the proxy configuration file at runtime"
    )
  set(OPENELP_DOC_HTMLHELP FALSE CACHE BOOL
    "Enable generation of Windows CHM help"
    )
endif()

#
# Dependencies
#

if(OPENELP_BUNDLE_PCRE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundlePCRE.cmake")
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PCRE REQUIRED libpcre2-8)
endif()

if(OPENELP_USE_OPENSSL)
  find_package(OpenSSL REQUIRED)
endif()

#
# Variables
#

set(OPENELP_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(OPENELP_DOC_DIR "${OPENELP_DIR}/doc")
set(OPENELP_INCLUDE_DIR "${OPENELP_DIR}/include")
set(OPENELP_PACKAGE_DIR "${OPENELP_DIR}/package")
set(OPENELP_SOURCE_DIR "${OPENELP_DIR}/src")
set(OPENELP_TEST_DIR "${OPENELP_DIR}/test")

set(OPENELP_MAJOR_VERSION 0)
set(OPENELP_MINOR_VERSION 6)
set(OPENELP_PATCH_VERSION 0)
set(OPENELP_VERSION
  ${OPENELP_MAJOR_VERSION}.${OPENELP_MINOR_VERSION}.${OPENELP_PATCH_VERSION})

#
# Flags
#

set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -DOPENELP_MAJOR_VERSION=${OPENELP_MAJOR_VERSION}")
set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -DOPENELP_MINOR_VERSION=${OPENELP_MINOR_VERSION}")
set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -DOPENELP_PATCH_VERSION=${OPENELP_PATCH_VERSION}")
set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -DOPENELP_VERSION=${OPENELP_VERSION}")

if(OPENELP_CONFIG_HINT)
  set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -DOPENELP_CONFIG_HINT=\"${OPENELP_CONFIG_HINT}\""
    )
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPCRE2_CODE_UNIT_WIDTH=8")

if(OPENELP_BUNDLE_PCRE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPCRE2_STATIC=1")
endif()

if(OPENELP_USE_EVENTLOG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_EVENTLOG=1")
endif()

if(OPENELP_USE_SYSLOG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SYSLOG=1")
endif()

if(OPENELP_USE_OPENSSL)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_OPENSSL=1")
endif()

if(WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /D_CRT_SECURE_NO_WARNINGS")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

#
# Includes
#

include_directories(${OPENELP_INCLUDE_DIR})

include_directories(${PCRE_INCLUDE_DIRS})

if(OPENELP_USE_OPENSSL)
  include_directories(${OPENSSL_INCLUDE_DIRS})
endif()

#
# Link Dirs
#

link_directories(${PCRE_LIBRARY_DIRS})

if(OPENELP_USE_OPENSSL)
  link_directories(${OPENSSL_LIB_DIRS})
endif()

#
# Subdirs
#

add_subdirectory(${OPENELP_DOC_DIR})
add_subdirectory(${OPENELP_INCLUDE_DIR})
add_subdirectory(${OPENELP_SOURCE_DIR})
add_subdirectory(${OPENELP_TEST_DIR})

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Package.cmake")
