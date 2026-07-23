add_executable(opensup
  main.cpp
  cli_parser.cpp
)

target_link_libraries(opensup
  PRIVATE
    opensup_core
    CLI11::CLI11
)

target_include_directories(opensup
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

set_project_warnings(opensup)
