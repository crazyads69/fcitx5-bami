configure_file(bami-addon.conf.in.in ${CMAKE_CURRENT_BINARY_DIR}/bami-addon.conf @ONLY)
configure_file(bami.conf.in ${CMAKE_CURRENT_BINARY_DIR}/bami.conf COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bami-addon.conf RENAME bami.conf
  DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bami.conf
  DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")
