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
    set(OPENELP_DOC_HTMLHELP_OUT_PATH
      "${CMAKE_CURRENT_BINARY_DIR}/html/openelp.chm"
      )
    set(OPENELP_DOC_HTMLHELP_CMD
      "${CMAKE_CURRENT_BINARY_DIR}/hhc.cmd"
      )

    string(REPLACE "/" "\\"
      OPENELP_DOC_HTMLHELP_OUT_PATH_BS
      "${OPENELP_DOC_HTMLHELP_OUT_PATH}"
      )
    string(REPLACE "/" "\\"
      OPENELP_DOC_HTMLHELP_PATH_BS
      "${OPENELP_DOC_HTMLHELP_PATH}"
      )
    string(REPLACE "/" "\\"
      OPENELP_DOC_HTMLHELP_CMD_BS
      "${OPENELP_DOC_HTMLHELP_CMD}"
      )

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/hhc.cmd.in"
      "${OPENELP_DOC_HTMLHELP_CMD}"
      @ONLY)

    set(OPENELP_DOC_ALL "ALL")
  else()
    set(OPENELP_DOC_HTMLHELP_YN "NO")
    set(OPENELP_DOC_SEARCH_YN "YES")
    set(OPENELP_DOC_HTMLHELP_OUT_PATH "")
    set(OPENELP_DOC_ALL "")
  endif()

  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    @ONLY)

  add_custom_command(OUTPUT
      ${OPENELP_DOC_HTMLHELP_OUT_PATH}
      "${CMAKE_CURRENT_BINARY_DIR}/html/index.html"
    COMMAND "${DOXYGEN_EXECUTABLE}"
    ARGS "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    MAIN_DEPENDENCY "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    DEPENDS
      "${OPENELP_DOC_HTMLHELP_CMD}"
      "${OPENELP_INCLUDE_DIR}/openelp/openelp.h"
      "${OPENELP_DIR}/CONTRIBUTING.md"
      "${OPENELP_DIR}/TODO.md"
      "${OPENELP_DIR}/README.md"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating API documentation with Doxygen" VERBATIM
    )


  add_custom_target(doc ${OPENELP_DOC_ALL}
    DEPENDS
      ${OPENELP_DOC_HTMLHELP_OUT_PATH}
      "${CMAKE_CURRENT_BINARY_DIR}/html/index.html"
    )
else()
  message("Could not find Doxygen - not generating API documentation")
endif()

#
# Install
#

install(FILES "ELProxy.conf"
  DESTINATION "${SYSCONF_INSTALL_DIR}"
  COMPONENT config
  )

if(UNIX)
  install(FILES openelpd.1
    DESTINATION "${MAN_INSTALL_DIR}/man1"
    COMPONENT docs
    )

  if(NOT APPLE)
    get_filename_component(FULL_BIN_INSTALL_DIR
      ${BIN_INSTALL_DIR} REALPATH BASE_DIR ${CMAKE_INSTALL_PREFIX})
    get_filename_component(FULL_SYSCONF_INSTALL_DIR
      ${SYSCONF_INSTALL_DIR} REALPATH BASE_DIR ${CMAKE_INSTALL_PREFIX})

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/openelp.service.in"
      "${CMAKE_CURRENT_BINARY_DIR}/openelp.service"
      @ONLY)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openelp.service"
      DESTINATION "${SYSTEMD_SERVICES_INSTALL_DIR}"
      COMPONENT systemd
      )
  endif()
endif()

if(OPENELP_DOC_HTMLHELP AND DOXYGEN_FOUND)
  install(FILES "${OPENELP_DOC_HTMLHELP_OUT_PATH}"
    DESTINATION "./"
    COMPONENT help
    )
endif()

add_subdirectory("${OPENELP_DOC_DIR}/icons")
