configure_file(bami-addon.conf.in.in ${CMAKE_CURRENT_BINARY_DIR}/bami-addon.conf @ONLY)
configure_file(bami.conf.in ${CMAKE_CURRENT_BINARY_DIR}/bami.conf COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bami-addon.conf RENAME bami.conf
  DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bami.conf
  DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")

# Optional session warm-up; installation does not enable or start it.
configure_file(fcitx5-bami-init.service.in ${CMAKE_CURRENT_BINARY_DIR}/fcitx5-bami-init.service @ONLY)
install(PROGRAMS ${CMAKE_SOURCE_DIR}/tools/fcitx5-bami-init DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fcitx5-bami-init.service
  DESTINATION "${CMAKE_INSTALL_DATADIR}/systemd/user")
install(FILES bami-panel.conf DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/bami/profiles")
