cmake_minimum_required(VERSION 3.22)
project(fcitx5-bami VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Lowest Fcitx the code is written against: Ubuntu 22.04 ships 5.0.14.
set(REQUIRED_FCITX_VERSION 5.0.14)

option(BUILD_TESTING "Build and register the tests" ON)

find_package(ECM 1.0.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(GNUInstallDirs)
include(FeatureSummary)

find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)
find_package(Fcitx5Config REQUIRED)
find_package(Fcitx5Utils REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend)
find_package(Threads REQUIRED)
find_program(GO_EXECUTABLE go REQUIRED)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_definitions(-DFCITX_GETTEXT_DOMAIN=\"fcitx5-bami\")

# ---- Go core: go build -buildmode=c-archive -> bami-core.a + bami-core.h ----
file(GLOB_RECURSE BAMI_GO_SRCS CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/bridge/*.go ${CMAKE_SOURCE_DIR}/engine/*.go)
list(FILTER BAMI_GO_SRCS EXCLUDE REGEX "_test\\.go$")
set(BAMI_GO_DIR ${CMAKE_BINARY_DIR}/go)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/bami-core.a ${CMAKE_BINARY_DIR}/bami-core.h
  DEPENDS ${BAMI_GO_SRCS} ${CMAKE_SOURCE_DIR}/go.mod
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  # GOCACHE/GOPATH inside the build tree: packaging sandboxes have no writable $HOME.
  COMMAND ${CMAKE_COMMAND} -E env CGO_ENABLED=1 CC=${CMAKE_C_COMPILER}
          GOCACHE=${BAMI_GO_DIR}/cache GOPATH=${BAMI_GO_DIR}/path GOFLAGS=-buildvcs=false
          GO111MODULE=on GOTOOLCHAIN=local
          ${GO_EXECUTABLE} build -buildmode=c-archive -trimpath
          -o ${CMAKE_BINARY_DIR}/bami-core.a ./bridge
  COMMENT "Building the Go core (c-archive)"
  VERBATIM)
add_custom_target(bami-core-build DEPENDS ${CMAKE_BINARY_DIR}/bami-core.a ${CMAKE_BINARY_DIR}/bami-core.h)
add_library(Bami::Core STATIC IMPORTED GLOBAL)
add_dependencies(Bami::Core bami-core-build)
set_target_properties(Bami::Core PROPERTIES
  IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/bami-core.a
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
  INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")

add_subdirectory(src)
add_subdirectory(data)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
