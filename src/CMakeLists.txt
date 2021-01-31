#
# Source configuration
#

if(UNIX)
  set(OPENELP_PLATFORM_FILES
    ${OPENELP_SOURCE_DIR}/mutex_posix.c
    ${OPENELP_SOURCE_DIR}/thread_posix.c
    )
  set(OPENELP_ICON_RESOURCE "")
elseif(WIN32)
  set(OPENELP_PLATFORM_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/eventlog/log_eventlog_messages.rc
    ${OPENELP_SOURCE_DIR}/conn_wsa_errno.c
    ${OPENELP_SOURCE_DIR}/mutex_win.c
    ${OPENELP_SOURCE_DIR}/thread_win.c
    )
  set(OPENELP_ICON_RESOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/eventlog/icon.rc"
    )
else()
  message(ERROR "Unsupported platform")
endif()

if(OPENELP_USE_OPENSSL)
  set(OPENELP_MD5_FILES)
else()
  set(OPENELP_MD5_FILES ${OPENELP_SOURCE_DIR}/md5.c)
endif()

#
# Targets
#

add_library(openelp_objects OBJECT
  ${OPENELP_SOURCE_DIR}/conf.c
  ${OPENELP_SOURCE_DIR}/conn.c
  ${OPENELP_SOURCE_DIR}/digest.c
  ${OPENELP_SOURCE_DIR}/log.c
  ${OPENELP_SOURCE_DIR}/proxy.c
  ${OPENELP_SOURCE_DIR}/proxy_conn.c
  ${OPENELP_SOURCE_DIR}/rand.c
  ${OPENELP_SOURCE_DIR}/regex.c
  ${OPENELP_SOURCE_DIR}/registration.c
  ${OPENELP_MD5_FILES}
  ${OPENELP_PLATFORM_FILES}
  )

if(WIN32)
  add_custom_command(OUTPUT
      "${CMAKE_CURRENT_BINARY_DIR}/eventlog/log_eventlog_messages.h"
      "${CMAKE_CURRENT_BINARY_DIR}/eventlog/log_eventlog_messages.rc"
    COMMAND mc ARGS
      -U "${OPENELP_SOURCE_DIR}/log_eventlog_messages.mc"
      -r "${CMAKE_CURRENT_BINARY_DIR}/eventlog"
      -h "${CMAKE_CURRENT_BINARY_DIR}/eventlog"
    MAIN_DEPENDENCY "${OPENELP_SOURCE_DIR}/log_eventlog_messages.mc"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/eventlog"
    )

  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/eventlog/icon.rc"
    "IDI_ICON1 ICON DISCARDABLE \"${OPENELP_DOC_DIR}/icons/windows.ico\""
    )

  target_include_directories(openelp_objects
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/eventlog"
    )
endif()

if(NOT WIN32 AND BUILD_SHARED_LIBS)
  target_compile_options(openelp_objects PRIVATE -fPIC)
endif()

add_library(openelp $<TARGET_OBJECTS:openelp_objects>)

add_executable(openelpd
  "${OPENELP_SOURCE_DIR}/proxyd.c"
  "${OPENELP_ICON_RESOURCE}")

if(WIN32)
  add_executable(openelp_service ${OPENELP_SOURCE_DIR}/service.c)
endif()

#
# Includes
#

target_include_directories(openelp_objects PUBLIC
  ${OPENELP_INCLUDE_DIR}
  )
target_include_directories(openelp_objects PRIVATE
  ${PCRE_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIRS}
  )
target_include_directories(openelpd PRIVATE
  ${OPENELP_INCLUDE_DIR}
  )

if(WIN32)
  target_include_directories(openelp_service PRIVATE
    ${OPENELP_INCLUDE_DIR}
    )
endif()

#
# Libraries
#

target_link_libraries(openelp PRIVATE ${PCRE_LIBRARIES})

if(UNIX)
  target_link_libraries(openelp PRIVATE pthread)
elseif(WIN32)
  target_link_libraries(openelp PRIVATE ws2_32 advapi32)
endif()

if(OPENELP_USE_OPENSSL)
  target_link_libraries(openelp PRIVATE ${OPENSSL_LIBRARIES})
endif()

target_link_libraries(openelpd PRIVATE openelp)

if(WIN32)
  target_link_libraries(openelp_service PRIVATE openelp)
endif()

#
# Properties
#

set_target_properties(openelp PROPERTIES
  SOVERSION ${OPENELP_MAJOR_VERSION}
  VERSION ${OPENELP_MAJOR_VERSION}.${OPENELP_MINOR_VERSION}.${OPENELP_PATCH_VERSION}
  )

if(WIN32)
  target_compile_options(openelp_objects PRIVATE
    "-DOPENELP_API=__declspec(dllexport)"
    )
else()
  target_compile_options(openelp_objects PRIVATE
    "-DOPENELP_API=__attribute__((__visibility__(\"default\")))"
    "-fvisibility=hidden"
    )
endif()

if(OPENELP_BUNDLE_PCRE)
  add_dependencies(openelp_objects pcre)
endif()

#
# Install
#

if(WIN32)
  install(TARGETS openelp
    RUNTIME DESTINATION bin
    COMPONENT libs
    )

  install(TARGETS openelp
    ARCHIVE DESTINATION "${LIB_INSTALL_DIR}"
    COMPONENT devel
    )
else()
  install(TARGETS openelp
    ARCHIVE DESTINATION "${LIB_INSTALL_DIR}"
    LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
    RUNTIME DESTINATION bin
    COMPONENT libs
    )
endif()

install(TARGETS openelpd
  RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
  COMPONENT app
  )

if(WIN32)
  install(TARGETS openelp_service
    RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
    COMPONENT service
    )
endif()
