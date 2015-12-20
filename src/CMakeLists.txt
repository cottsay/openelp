#
# Source configuration
#

if(UNIX)
  set(OPENELP_PLATFORM_FILES
    ${OPENELP_SOURCE_DIR}/mutex_posix.c
    ${OPENELP_SOURCE_DIR}/thread_posix.c
    )
elseif(WIN32)
  set(OPENELP_PLATFORM_FILES
    ${OPENELP_SOURCE_DIR}/mutex_win.c
    ${OPENELP_SOURCE_DIR}/thread_win.c
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
  ${OPENELP_SOURCE_DIR}/rand.c
  ${OPENELP_MD5_FILES}
  ${OPENELP_PLATFORM_FILES}
  )

add_executable(openelpd ${OPENELP_SOURCE_DIR}/proxyd.c)

#
# Libraries
#

if(UNIX)
  target_link_libraries(openelp pthread)
elseif(WIN32)
  target_link_libraries(openelp ws2_32)
endif()

target_link_libraries(openelpd openelp)

if(OPENELP_USE_OPENSSL)
  target_link_libraries(openelp ${OPENSSL_LIBRARIES})
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
