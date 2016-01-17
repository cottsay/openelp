#
# Dependencies
#

find_package(Doxygen)

#
# Targets
#

if(DOXYGEN_FOUND)
  set(OPENELP_DOC_SOURCES "\"${OPENELP_INCLUDE_DIR}/openelp/openelp.h\" \\
    \"${OPENELP_DIR}/CONTRIBUTING.md\" \\
    \"${OPENELP_DIR}/TODO.md\" \\
    \"${OPENELP_DIR}/README.md\"")

  if(OPENELP_DOC_INTERNAL)
    set(OPENELP_DOC_SOURCES "${OPENELP_DOC_SOURCES} \\
      \"${OPENELP_INCLUDE_DIR}\" \\
      \"${OPENELP_SOURCE_DIR}\" \\
      \"${OPENELP_TEST_DIR}\"")
    set(OPENELP_DOC_INTERNAL_YN "YES")
  else()
    set(OPENELP_DOC_INTERNAL_YN "NO")
  endif()

  if(OPENELP_DOC_HTMLHELP)
    set(OPENELP_DOC_HTMLHELP_YN "YES")
    set(OPENELP_DOC_SEARCH_YN "NO")
  else()
    set(OPENELP_DOC_HTMLHELP_YN "NO")
    set(OPENELP_DOC_SEARCH_YN "YES")
  endif()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    @ONLY)

  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
else()
  message("Could not find Doxygen - not generating API documentation")
endif()

#
# Install
#

install(FILES ELProxy.conf
  DESTINATION "${SYSCONF_INSTALL_DIR}"
  COMPONENT config
  )

if(UNIX)
  install(FILES openelpd.1
    DESTINATION "${MAN_INSTALL_DIR}/man1"
    COMPONENT docs
    )

  install(FILES openelp.service
    DESTINATION "${SYSTEMD_SERVICES_INSTALL_DIR}"
    COMPONENT systemd
    )
endif()

if(OPENELP_DOC_HTMLHELP)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/html/openelp.chm"
    DESTINATION "./"
    COMPONENT help
    )
endif()

add_subdirectory("${OPENELP_DOC_DIR}/icons")
