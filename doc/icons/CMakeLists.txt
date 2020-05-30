#
# Targets
#

set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM 1)

add_custom_command(OUTPUT "${OPENELP_DOC_DIR}/icons/24x24.png"
  COMMAND convert
  ARGS -background none -resize 24x24
    "${OPENELP_DOC_DIR}/icons/scalable.svg"
    "${OPENELP_DOC_DIR}/icons/24x24.png"
  MAIN_DEPENDENCY "${OPENELP_DOC_DIR}/icons/scalable.svg"
  )
add_custom_command(OUTPUT "${OPENELP_DOC_DIR}/icons/48x48.png"
  COMMAND convert
  ARGS -background none -resize 48x48
    "${OPENELP_DOC_DIR}/icons/scalable.svg"
    "${OPENELP_DOC_DIR}/icons/48x48.png"
  MAIN_DEPENDENCY "${OPENELP_DOC_DIR}/icons/scalable.svg"
  )
add_custom_command(OUTPUT "${OPENELP_DOC_DIR}/icons/96x96.png"
  COMMAND convert
  ARGS -background none -resize 96x96
    "${OPENELP_DOC_DIR}/icons/scalable.svg"
    "${OPENELP_DOC_DIR}/icons/96x96.png"
  MAIN_DEPENDENCY "${OPENELP_DOC_DIR}/icons/scalable.svg"
  )
add_custom_command(OUTPUT "${OPENELP_DOC_DIR}/icons/256x256.png"
  COMMAND convert
  ARGS -background none -resize 256x256
    "${OPENELP_DOC_DIR}/icons/scalable.svg"
    "${OPENELP_DOC_DIR}/icons/256x256.png"
  MAIN_DEPENDENCY "${OPENELP_DOC_DIR}/icons/scalable.svg"
  )
add_custom_command(OUTPUT "${OPENELP_DOC_DIR}/icons/installer.bmp"
  COMMAND convert
  ARGS -background white -alpha remove -alpha off
    "${OPENELP_DOC_DIR}/icons/scalable.svg"
    -resize x57 -gravity center -extent 150x57
    BMP3:"${OPENELP_DOC_DIR}/icons/installer.bmp"
  MAIN_DEPENDENCY "${OPENELP_DOC_DIR}/icons/scalable.svg"
  )
add_custom_command(OUTPUT "${OPENELP_DOC_DIR}/icons/windows.ico"
  COMMAND convert
    "${OPENELP_DOC_DIR}/icons/24x24.png"
    "${OPENELP_DOC_DIR}/icons/48x48.png"
    "${OPENELP_DOC_DIR}/icons/96x96.png"
    "${OPENELP_DOC_DIR}/icons/256x256.png"
    "${OPENELP_DOC_DIR}/icons/windows.ico"
  DEPENDS
    "${OPENELP_DOC_DIR}/icons/24x24.png"
    "${OPENELP_DOC_DIR}/icons/48x48.png"
    "${OPENELP_DOC_DIR}/icons/96x96.png"
    "${OPENELP_DOC_DIR}/icons/256x256.png"
  )

add_custom_target(icons
  DEPENDS
    "${OPENELP_DOC_DIR}/icons/24x24.png"
    "${OPENELP_DOC_DIR}/icons/48x48.png"
    "${OPENELP_DOC_DIR}/icons/96x96.png"
    "${OPENELP_DOC_DIR}/icons/256x256.png"
    "${OPENELP_DOC_DIR}/icons/installer.bmp"
    "${OPENELP_DOC_DIR}/icons/windows.ico"
  )
