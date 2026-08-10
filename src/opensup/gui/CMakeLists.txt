set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Generate resources.rc with absolute path to the icon
set(OPENSUP_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/OpenSup.ico")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources.rc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
               @ONLY)

add_executable(OpenSUP
  main_window.cpp
  encode_worker.cpp
  qt_log_handler.cpp
  theme_manager.cpp
  main.cpp
  main_window.ui
  resources.qrc
  "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
)

target_include_directories(OpenSUP
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/OpenSUP_autogen/include
    ${CMAKE_BINARY_DIR}
)

if(MINGW)
  # Embed icon for Windows taskbar/explorer
  set_target_properties(OpenSUP PROPERTIES
    LINK_FLAGS "-Wl,--subsystem,windows"
  )

  # Deploy Qt + MinGW runtime DLLs alongside the .exe
  set(QT_MINGW_BIN "/usr/x86_64-w64-mingw32/sys-root/mingw/bin")
  set(QT_MINGW_PLUGINS "/usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt6/plugins")

  add_custom_command(TARGET OpenSUP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:OpenSUP>/platforms"
    COMMENT "Creating platforms/ directory"
  )

  # Qt6 DLLs
  foreach(QT_DLL Qt6Core Qt6Gui Qt6Widgets)
    add_custom_command(TARGET OpenSUP POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${QT_MINGW_BIN}/${QT_DLL}.dll"
          "$<TARGET_FILE_DIR:OpenSUP>/"
      COMMENT "Deploying ${QT_DLL}.dll"
    )
  endforeach()

  # MinGW runtime DLLs
  foreach(MINGW_DLL libgcc_s_seh-1 libstdc++-6 libwinpthread-1)
    add_custom_command(TARGET OpenSUP POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${QT_MINGW_BIN}/${MINGW_DLL}.dll"
          "$<TARGET_FILE_DIR:OpenSUP>/"
      COMMENT "Deploying ${MINGW_DLL}.dll"
    )
  endforeach()

  # Qt6Core transitive deps
  foreach(CORE_DLL icui18n76 icuuc76 icudata76 libpcre2-16-0 zlib1)
    add_custom_command(TARGET OpenSUP POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${QT_MINGW_BIN}/${CORE_DLL}.dll"
          "$<TARGET_FILE_DIR:OpenSUP>/"
      COMMENT "Deploying ${CORE_DLL}.dll"
    )
  endforeach()

  # Qt6Gui transitive deps
  foreach(GUI_DLL libfontconfig-1 libfreetype-6 libharfbuzz-0 libpng16-16
                  libbz2-1 libexpat-1 libglib-2.0-0 libintl-8 iconv libpcre2-8-0)
    add_custom_command(TARGET OpenSUP POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${QT_MINGW_BIN}/${GUI_DLL}.dll"
          "$<TARGET_FILE_DIR:OpenSUP>/"
      COMMENT "Deploying ${GUI_DLL}.dll"
    )
  endforeach()

  # Windows platform plugin
  add_custom_command(TARGET OpenSUP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_MINGW_PLUGINS}/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:OpenSUP>/platforms/"
    COMMENT "Deploying platforms/qwindows.dll"
  )
endif()

target_link_libraries(OpenSUP
  PRIVATE
    opensup_core
    Qt6::Widgets
)
