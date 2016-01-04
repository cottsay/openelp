#
# Dependencies
#

find_package(Doxygen)

#
# Targets
#

if(DOXYGEN_FOUND)
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
