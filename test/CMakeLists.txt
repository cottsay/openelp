enable_testing()

#
# Source configuration
#

if(OPENELP_USE_OPENSSL)
  set(OPENELP_MD5_FILES)
else()
  set(OPENELP_MD5_FILES ${OPENELP_SOURCE_DIR}/md5.c)
endif()

#
# Targets
#

add_executable(test_md5
  test_md5.c
  ${OPENELP_SOURCE_DIR}/digest.c
  ${OPENELP_MD5_FILES}
  )

add_executable(test_proxy
  test_proxy.c
  )

add_executable(test_regex
  test_regex.c
  ${OPENELP_SOURCE_DIR}/regex.c
  )

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS test_md5 test_proxy test_regex)

#
# Libraries
#

if(OPENELP_USE_OPENSSL)
  target_link_libraries(test_md5 ${OPENSSL_LIBRARIES})
endif()

target_link_libraries(test_proxy
  openelp
  )

target_link_libraries(test_regex
  ${PCRE_LIBRARIES}
  )

#
# Tests
#

add_test(test_md5 test_md5)
add_test(test_proxy test_proxy)
add_test(test_regex test_regex)
