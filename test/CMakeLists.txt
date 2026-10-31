include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES Fcitx5::Utils)
check_cxx_source_compiles("
#include <fcitx-utils/testing.h>
int main() { fcitx::setupTestingEnvironmentPath(\"/\", {}, {}); }" BAMI_HAVE_TESTING_PATH)
unset(CMAKE_REQUIRED_LIBRARIES)
configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)
if(BAMI_HAVE_TESTING_PATH)
  add_compile_definitions(BAMI_HAVE_TESTING_PATH=1)
endif()

# Data dirs for the instance test: <dir>/addon/bami.conf, <dir>/inputmethod/bami.conf
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/addon ${CMAKE_CURRENT_BINARY_DIR}/inputmethod)
configure_file(${CMAKE_BINARY_DIR}/data/bami-addon.conf ${CMAKE_CURRENT_BINARY_DIR}/addon/bami.conf COPYONLY)
configure_file(${CMAKE_BINARY_DIR}/data/bami.conf ${CMAKE_CURRENT_BINARY_DIR}/inputmethod/bami.conf COPYONLY)

# Pure strategy table: needs no Fcitx runtime and no Go.
add_executable(teststrategy teststrategy.cpp ${CMAKE_SOURCE_DIR}/src/strategy.cpp)
target_include_directories(teststrategy PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME teststrategy COMMAND teststrategy)

# Composer against a fake application (FakeIC) and the real Go core.
add_executable(testcomposer testcomposer.cpp)
target_include_directories(testcomposer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(testcomposer bami-logic)
add_test(NAME testcomposer COMMAND testcomposer)

# Whole addon inside a real Instance with the test frontend.
add_executable(testbami testbami.cpp)
target_include_directories(testbami PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(testbami Fcitx5::Core Fcitx5::Utils Fcitx5::Module::TestFrontend)
add_dependencies(testbami bami)
add_test(NAME testbami COMMAND testbami)

# C ABI ownership, error containment, and generated-header compatibility.
add_executable(testbridge testbridge.cpp)
target_link_libraries(testbridge Bami::Core)
add_test(NAME testbridge COMMAND testbridge)

add_test(NAME testsessioninit COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/testsessioninit.sh
  ${CMAKE_SOURCE_DIR}/tools/fcitx5-bami-init)
