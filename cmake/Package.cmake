#
# CPack Configuration
#

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open Source EchoLink Proxy")

set(CPACK_PACKAGE_EXECUTABLES "openelpd;openelpd")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenELP")
set(CPACK_PACKAGE_NAME "OpenELP")
set(CPACK_PACKAGE_VENDOR "Scott K Logan (KM0H)")
set(CPACK_PACKAGE_CONTACT "logans@cottsay.net")

set(CPACK_PACKAGE_VERSION_MAJOR "${OPENELP_MAJOR_VERSION}")
set(CPACK_PACKAGE_VERSION_MINOR "${OPENELP_MINOR_VERSION}")
set(CPACK_PACKAGE_VERSION_PATCH "${OPENELP_PATCH_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${OPENELP_DIR}/LICENSE")

set(CPACK_COMPONENTS_ALL app config devel libs license)
set(CPACK_COMPONENT_LICENSE_HIDDEN TRUE)
set(CPACK_COMPONENT_APP_DISPLAY_NAME "Executable Application")
set(CPACK_COMPONENT_CONFIG_DISPLAY_NAME "Sample Configuration")
set(CPACK_COMPONENT_LIBS_DISPLAY_NAME "Proxy Library")
set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_APP_DESCRIPTION
  "Simple executable application which opens an EchoLink proxy")
set(CPACK_COMPONENT_CONFIG_DESCRIPTION
  "Sample configuration file for customizing an EchoLink proxy")
set(CPACK_COMPONENT_LIBS_DESCRIPTION
  "Library used by applications to set up and control an EchoLink proxy")
set(CPACK_COMPONENT_DEVEL_DESCRIPTION
  "C headers and library files used by other projects to build using OpenELP")
set(CPACK_COMPONENT_APP_DEPENDS libs)
set(CPACK_COMPONENT_DEVEL_DEPENDS libs)
set(CPACK_COMPONENT_DEVEL_DISABLED True)

set(CPACK_NSIS_URL_INFO_ABOUT "http://github.com/cottsay/openelp")

include(CPack)

#
# Licenses
#

if(WIN32)
  install(FILES "${OPENELP_DIR}/LICENSE"
    DESTINATION "./"
    COMPONENT license
    RENAME "LICENSE.txt"
    )

  if(OPENELP_BUNDLE_PCRE)
    add_custom_command(TARGET pcre POST_BUILD
      COMMAND ${CMAKE_COMMAND} -P ${OPENELP_DIR}/cmake/unix2dos.cmake
        "${OPENELP_PCRE_LICENSE_PATH}"
        "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.pcre"
      )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.pcre"
      DESTINATION "./"
      COMPONENT license
      RENAME "LICENSE.pcre.txt"
      )
  endif()
endif()
