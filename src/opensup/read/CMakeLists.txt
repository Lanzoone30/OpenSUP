add_library(opensup_read
  sup_reader.cpp
  inspect.cpp
)

target_include_directories(opensup_read
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(opensup_read
  PUBLIC
    opensup_core
)

target_precompile_headers(opensup_read
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h
)

set_project_warnings(opensup_read)
