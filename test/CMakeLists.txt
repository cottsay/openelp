#
# Targets
#

add_executable(test_digest test_digest.c $<TARGET_OBJECTS:openelp_objects>)
add_executable(test_md5 test_md5.c $<TARGET_OBJECTS:openelp_objects>)
add_executable(test_proxy test_proxy.c $<TARGET_OBJECTS:openelp_objects>)
add_executable(test_regex test_regex.c $<TARGET_OBJECTS:openelp_objects>)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --force-new-ctest-process $(ARGS)
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
  COMMENT "Running tests..."
  DEPENDS
    test_digest
    test_md5
    test_proxy
    test_regex)

if(WIN32)
  set_target_properties(test_proxy PROPERTIES
    COMPILE_FLAGS "/DOPENELP_API=__declspec(dllexport)"
    )
endif()

#
# Libraries
#

get_target_property(OPENELP_LIBRARIES openelp LINK_LIBRARIES)
target_link_libraries(test_digest ${OPENELP_LIBRARIES})
target_link_libraries(test_md5 ${OPENELP_LIBRARIES})
target_link_libraries(test_proxy ${OPENELP_LIBRARIES})
target_link_libraries(test_regex ${OPENELP_LIBRARIES})

#
# Tests
#

add_test(NAME test_digest COMMAND test_digest)
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
  test_digest
  test_md5
  test_proxy
  test_regex
  PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
