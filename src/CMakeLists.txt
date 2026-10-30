# Logic shared by the addon and the tests (everything except the Fcitx factory).
add_library(bami-logic STATIC strategy.cpp composer.cpp)
set_target_properties(bami-logic PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(bami-logic PUBLIC Fcitx5::Core Fcitx5::Config Fcitx5::Utils Bami::Core)
target_include_directories(bami-logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(bami-logic bami-core-build)

if(COMMAND add_fcitx5_addon)
  add_fcitx5_addon(bami bami.cpp)
else()
  add_library(bami MODULE bami.cpp) # Fcitx 5.0.14 has no add_fcitx5_addon
endif()
target_link_libraries(bami PRIVATE bami-logic)
# Keep the Go runtime's symbols out of the process-wide dynamic symbol table.
target_link_options(bami PRIVATE LINKER:--exclude-libs,ALL)
install(TARGETS bami DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
