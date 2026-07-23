add_library(opensup_media
  palette.cpp
  pgraphics.cpp
  pgstream.cpp
  optimizer.cpp
)

target_include_directories(opensup_media
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_include_directories(opensup_media
  PRIVATE
    ${CMAKE_SOURCE_DIR}/extern
)

target_link_libraries(opensup_media
  PUBLIC
    opensup_common
    imagequant
)

target_precompile_headers(opensup_media
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h
)

set_project_warnings(opensup_media)
