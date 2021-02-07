project(openelp C)

cmake_minimum_required(VERSION 3.4)

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
  set(SYSCONF_INSTALL_DIR "etc" CACHE PATH
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

# Make sure we have CMAKE_BUILD_TYPE set to something
if(NOT CMAKE_BUILD_TYPE)
  list(GET CMAKE_CONFIGURATION_TYPES 0 CMAKE_BUILD_TYPE)
endif()

set(OPENELP_EXTRA_VERSION "" CACHE STRING
  "Extra value suffixed to the version number"
  )

if(IS_ABSOLUTE ${SYSCONF_INSTALL_DIR})
  file(TO_CMAKE_PATH
    "${SYSCONF_INSTALL_DIR}/ELProxy.conf"
    OPENELP_CONFIG_HINT_DEFAULT)
elseif(IS_ABSOLUTE ${BIN_INSTALL_DIR})
  file(RELATIVE_PATH OPENELP_CONFIG_HINT_DEFAULT
    "${BIN_INSTALL_DIR}"
    "${CMAKE_PREFIX_PATH}/${SYSCONF_INSTALL_DIR}/ELProxy.conf")
else()
  file(RELATIVE_PATH OPENELP_CONFIG_HINT_DEFAULT
    "${CMAKE_PREFIX_PATH}/${BIN_INSTALL_DIR}"
    "${CMAKE_PREFIX_PATH}/${SYSCONF_INSTALL_DIR}/ELProxy.conf")
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL
  "Indicates that any libraries which are produced should be shared (.dll/.so)"
  )
set(OPENELP_DOC_INTERNAL FALSE CACHE BOOL
  "Include internal code when documenting API"
  )
set(OPENELP_USE_OPENSSL FALSE CACHE BOOL
  "Use OpenSSL for MD5 computation instead of bundled md5.c"
  )
set(OPENELP_CONFIG_HINT ${OPENELP_CONFIG_HINT_DEFAULT} CACHE PATH
  "Hint path when searching for the proxy configuration file at runtime"
  )
if(WIN32)
  set(OPENELP_BUNDLE_PCRE TRUE CACHE BOOL
    "Download and compile PCRE2 instead of using the system's libpcre2-32"
    )
  set(OPENELP_USE_EVENTLOG TRUE CACHE BOOL
    "Enable support for logging to the Windows Event Log"
    )
  set(OPENELP_USE_SYSLOG FALSE CACHE BOOL
    "Enable support for logging to SYSLOG"
    )
  set(OPENELP_DOC_HTMLHELP TRUE CACHE BOOL
    "Enable generation of Windows CHM help"
    )
else()
  if(APPLE)
    set(OPENELP_BUNDLE_PCRE TRUE CACHE BOOL
      "Download and compile PCRE2 instead of using the system's libpcre2-32"
      )
  else()
    set(OPENELP_BUNDLE_PCRE FALSE CACHE BOOL
      "Download and compile PCRE2 instead of using the system's libpcre2-32"
      )
  endif()
  set(OPENELP_USE_EVENTLOG FALSE CACHE BOOL
    "Enable support for logging to the Windows Event Log"
    )
  set(OPENELP_USE_SYSLOG TRUE CACHE BOOL
    "Enable support for logging to SYSLOG"
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

if(OPENELP_DOC_HTMLHELP)
  find_program(OPENELP_DOC_HTMLHELP_PATH hhc
    PATHS
      "$ENV{ProgramW6432}\\HTML Help Workshop"
      "$ENV{ProgramFiles}\\HTML Help Workshop"
    DOC "HTML Help Compiler Command"
    )
  if(NOT OPENELP_DOC_HTMLHELP_PATH)
    message(FATAL_ERROR "Failed to find HTML Help Compiler (hhc)")
  endif()
endif()

if(MSVC)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT runtime)
  include(InstallRequiredSystemLibraries)
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
set(OPENELP_MINOR_VERSION 9)
set(OPENELP_PATCH_VERSION 0)
set(OPENELP_VERSION
  ${OPENELP_MAJOR_VERSION}.${OPENELP_MINOR_VERSION}.${OPENELP_PATCH_VERSION})
if(OPENELP_EXTRA_VERSION)
  set(OPENELP_VERSION "${OPENELP_VERSION}-${OPENELP_EXTRA_VERSION}")
endif()

#
# Flags
#

add_compile_options(
  -DOPENELP_MAJOR_VERSION=${OPENELP_MAJOR_VERSION}
  -DOPENELP_MINOR_VERSION=${OPENELP_MINOR_VERSION}
  -DOPENELP_PATCH_VERSION=${OPENELP_PATCH_VERSION}
  -DOPENELP_VERSION=${OPENELP_VERSION}
  )

if(OPENELP_CONFIG_HINT)
  add_compile_options(
    -DOPENELP_CONFIG_HINT=${OPENELP_CONFIG_HINT}
    )
endif()

add_compile_options(
  -DPCRE2_CODE_UNIT_WIDTH=8
  )

if(OPENELP_BUNDLE_PCRE)
  add_compile_options(
    -DPCRE2_STATIC=1
    )
endif()

if(OPENELP_USE_EVENTLOG)
  add_compile_options(
    -DHAVE_EVENTLOG=1
    )
endif()

if(OPENELP_USE_SYSLOG)
  add_compile_options(
    -DHAVE_SYSLOG=1
    )
endif()

if(OPENELP_USE_OPENSSL)
  add_compile_options(
    -DHAVE_OPENSSL=1
    )
endif()

if(WIN32)
  add_compile_options(
    /W3
    /D_CRT_SECURE_NO_WARNINGS
    /DWIN32_LEAN_AND_MEAN
    )
else()
  add_compile_options(
    -Wall
    -Wextra
    -Wunused-macros
    -Wunreachable-code
    )
  include(CheckCCompilerFlag)
  check_c_compiler_flag(-Wpedantic HAVE_W_PEDANTIC)
  if(HAVE_W_PEDANTIC)
    add_compile_options(-Wpedantic)
  endif()
  check_c_compiler_flag(-Wint-conversion HAVE_W_INT_CONVERSION)
  if(HAVE_W_INT_CONVERSION)
    add_compile_options(-Wint-conversion)
  endif()
endif()

# Default to C90
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 90)
endif()

#
# Link Dirs
#

link_directories(
  ${PCRE_LIBRARY_DIRS}
  ${OPENSSL_LIB_DIRS}
  )

#
# Subdirs
#

add_subdirectory(${OPENELP_DOC_DIR})
add_subdirectory(${OPENELP_INCLUDE_DIR})
add_subdirectory(${OPENELP_SOURCE_DIR})

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(${OPENELP_TEST_DIR})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Package.cmake")
