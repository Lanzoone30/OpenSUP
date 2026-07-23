set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(opensup_gui
  main_window.cpp
  encode_worker.cpp
  qt_log_handler.cpp
  main.cpp
  main_window.ui
)

target_include_directories(opensup_gui
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(opensup_gui
  PRIVATE
    opensup_core
    Qt6::Widgets
)
