enable_testing()

add_executable(test_md5
  test_md5.c
  )

target_link_libraries(test_md5
  openelp
  )

add_executable(test_proxy
  test_proxy.c
  )

target_link_libraries(test_proxy
  openelp
  )

add_test(test_md5 test_md5)
add_test(test_proxy test_proxy)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS test_md5 test_proxy)
