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

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --force-new-ctest-process $(ARGS)
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
  COMMENT "Running tests..."
  DEPENDS test_md5 test_proxy test_regex)

#
# Libraries
#

if(OPENELP_USE_OPENSSL)
  target_link_libraries(test_md5 ${OPENSSL_LIBRARIES})
endif()
if(WIN32)
  target_link_libraries(test_md5 ws2_32)
endif()

target_link_libraries(test_proxy
  openelp
  )

target_link_libraries(test_regex
  ${PCRE_LIBRARIES}
  )

if(UNIX)
  target_link_libraries(test_regex pthread)
endif()

#
# Properties
#

if(OPENELP_BUNDLE_PCRE)
  add_dependencies(test_regex pcre)
endif()

#
# Tests
#

add_test(NAME test_md5 COMMAND test_md5)
add_test(NAME test_proxy COMMAND test_proxy)
add_test(NAME test_regex COMMAND test_regex)

if(WIN32)
  set(PATH_SEPARATOR "\\;")
else()
  set(PATH_SEPARATOR ":")
endif()
set(TEST_ENVIRONMENT
  "PATH=$<JOIN:$<SHELL_PATH:$<TARGET_FILE_DIR:openelp>>;$ENV{PATH},${PATH_SEPARATOR}>"
  "LD_LIBRARY_PATH=$<JOIN:$<SHELL_PATH:$<TARGET_FILE_DIR:openelp>>;$ENV{LD_LIBRARY_PATH},${PATH_SEPARATOR}>"
  "DYLD_LIBRARY_PATH=$<JOIN:$<SHELL_PATH:$<TARGET_FILE_DIR:openelp>>;$ENV{DYLD_LIBRARY_PATH},${PATH_SEPARATOR}>"
  )

set_tests_properties(
  test_md5
  test_proxy
  test_regex
  PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
