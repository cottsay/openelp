enable_testing()

#
# Targets
#

add_executable(test_md5
  test_md5.c
  )

add_executable(test_proxy
  test_proxy.c
  )

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS test_md5 test_proxy)

#
# Libraries
#

target_link_libraries(test_md5
  openelp
  )

target_link_libraries(test_proxy
  openelp
  )

#
# Tests
#

add_test(test_md5 test_md5)
add_test(test_proxy test_proxy)
