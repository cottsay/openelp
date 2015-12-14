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

add_library(openelp
  ${OPENELP_SOURCE_DIR}/conf.c
  ${OPENELP_SOURCE_DIR}/conn.c
  ${OPENELP_SOURCE_DIR}/digest.c
  ${OPENELP_SOURCE_DIR}/log.c
  ${OPENELP_SOURCE_DIR}/md5.c
  ${OPENELP_SOURCE_DIR}/proxy.c
  ${OPENELP_SOURCE_DIR}/rand.c
  ${OPENELP_PLATFORM_FILES}
  )

if(UNIX)
  target_link_libraries(openelp
    pthread
    )
elseif(WIN32)
  target_link_libraries(openelp
    ws2_32
    )
endif()

if(OPENELP_USE_OPENSSL)
  target_link_libraries(openelp
    ${OPENSSL_LIBRARIES}
    )
endif()

set_target_properties(openelp PROPERTIES
  SOVERSION ${OPENELP_MAJOR_VERSION}
  VERSION ${OPENELP_VERSION}
  )

if(UNIX)
  install(TARGETS openelp
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    )
endif()

add_executable(openelpd
  ${OPENELP_SOURCE_DIR}/proxyd.c
  )

target_link_libraries(openelpd
  openelp
  )

if(UNIX)
  install(TARGETS openelpd
    RUNTIME DESTINATION bin
    )
endif()
