add_executable(opensup_cli
  main.cpp
  cli_parser.cpp
)

target_link_libraries(opensup_cli
  PRIVATE
    opensup_core
    CLI11::CLI11
)

target_include_directories(opensup_cli
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_BINARY_DIR}
)

set_project_warnings(opensup_cli)
