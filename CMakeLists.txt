project(openelp)
cmake_minimum_required(VERSION 2.7)

if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS "on")
endif()

if(NOT DEFINED OPENELP_USE_OPENSSL)
  set(OPENELP_USE_OPENSSL false)
endif()

if(NOT DEFINED SHARE_INSTALL_PREFIX)
  set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
endif()

if(OPENELP_USE_OPENSSL)
  find_package(OpenSSL REQUIRED)

  include_directories(
    ${OPENSSL_INCLUDE_DIRS}
    )

  link_directories(
    ${OPENSSL_LIB_DIRS}
    )
endif()

set(OPENELP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(OPENELP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(OPENELP_MAJOR_VERSION 0)
set(OPENELP_MINOR_VERSION 1)
set(OPENELP_PATCH_VERSION 0)
set(OPENELP_VERSION ${OPENELP_MAJOR_VERSION}.${OPENELP_MINOR_VERSION}.${OPENELP_PATCH_VERSION})

include_directories(
  ${OPENELP_INCLUDE_DIR}
  ${OPENELP_SOURCE_DIR}
  )

if(UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /WX /D_CRT_SECURE_NO_WARNINGS")
endif()

add_subdirectory(doc)
add_subdirectory(${OPENELP_SOURCE_DIR})
add_subdirectory(test)

