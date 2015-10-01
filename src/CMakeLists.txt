add_library(openelp
  ${OPENELP_SOURCE_DIR}/conn.c
  ${OPENELP_SOURCE_DIR}/digest.c
  ${OPENELP_SOURCE_DIR}/md5.c
  ${OPENELP_SOURCE_DIR}/mutex_posix.c
  ${OPENELP_SOURCE_DIR}/proxy.c
  ${OPENELP_SOURCE_DIR}/rand.c
  ${OPENELP_SOURCE_DIR}/thread_posix.c
  )

add_executable(openelpd
  ${OPENELP_SOURCE_DIR}/proxyd.c
  )

target_link_libraries(openelp
  pthread
  )

if(OPENELP_USE_OPENSSL)
  target_link_libraries(openelp
    ${OPENSSL_LIBRARIES}
    )
endif()

set_target_properties(openelp PROPERTIES
  SOVERSION ${OPENELP_MAJOR_VERSION}
  VERSION ${OPENELP_VERSION}
  )
target_link_libraries(openelpd
  openelp
  )

install(TARGETS openelp openelpd
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib${LIB_SUFFIX}
  )
