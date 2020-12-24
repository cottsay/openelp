macro(add_openelp_test test_name)
  add_executable(${test_name} ${ARGN} $<TARGET_OBJECTS:openelp_objects>)

  get_target_property(OPENELP_INCLUDE_DIRECTORIES openelp_objects INCLUDE_DIRECTORIES)
  target_include_directories(${test_name} PRIVATE ${OPENELP_INCLUDE_DIRECTORIES})

  get_target_property(OPENELP_LIBRARIES openelp LINK_LIBRARIES)
  target_link_libraries(${test_name} ${OPENELP_LIBRARIES})

  if(WIN32)
    target_compile_options(${test_name} PRIVATE
      "-DOPENELP_API=__declspec(dllexport)"
      )
  endif()

  add_test(NAME ${test_name} COMMAND ${test_name})

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
    ${test_name}
    PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

  list(APPEND OPENELP_TEST_TARGETS ${test_name})
endmacro()

add_openelp_test(test_digest test_digest.c)
add_openelp_test(test_md5 test_md5.c)
add_openelp_test(test_proxy test_proxy.c)
add_openelp_test(test_regex test_regex.c)
