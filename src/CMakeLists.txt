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

add_library(openelp
  ${OPENELP_SOURCE_DIR}/conf.c
  ${OPENELP_SOURCE_DIR}/conn.c
  ${OPENELP_SOURCE_DIR}/digest.c
  ${OPENELP_SOURCE_DIR}/log.c
  ${OPENELP_SOURCE_DIR}/proxy.c
  ${OPENELP_SOURCE_DIR}/proxy_conn.c
  ${OPENELP_SOURCE_DIR}/rand.c
  ${OPENELP_SOURCE_DIR}/regex.c
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

  target_include_directories(openelp
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/eventlog"
    )
endif()

add_executable(openelpd
  "${OPENELP_SOURCE_DIR}/proxyd.c"
  "${OPENELP_ICON_RESOURCE}")

if(WIN32)
  add_executable(openelp_service ${OPENELP_SOURCE_DIR}/service.c)
endif()

#
# Libraries
#

target_link_libraries(openelp ${PCRE_LIBRARIES})

if(UNIX)
  target_link_libraries(openelp pthread)
elseif(WIN32)
  target_link_libraries(openelp ws2_32 advapi32)
endif()

if(OPENELP_USE_OPENSSL)
  target_link_libraries(openelp ${OPENSSL_LIBRARIES})
endif()

target_link_libraries(openelpd openelp)

if(WIN32)
  target_link_libraries(openelp_service openelp)
endif()

#
# Properties
#

set_target_properties(openelp PROPERTIES
  SOVERSION ${OPENELP_MAJOR_VERSION}
  VERSION ${OPENELP_VERSION}
  )

if(WIN32)
  set_target_properties(openelp PROPERTIES
    COMPILE_FLAGS "/DOPENELP_API=__declspec(dllexport)"
    )
endif()

if(OPENELP_BUNDLE_PCRE)
  add_dependencies(openelp pcre)
endif()

#
# Install
#

install(TARGETS openelp
  LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
  RUNTIME DESTINATION bin
  COMPONENT libs
  )

if(WIN32)
  install(TARGETS openelp
    ARCHIVE DESTINATION "${LIB_INSTALL_DIR}"
    COMPONENT devel
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
