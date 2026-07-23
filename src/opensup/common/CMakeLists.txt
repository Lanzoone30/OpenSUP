add_library(opensup_common
  error.cpp
  memory.cpp
  geometry.cpp
  bdvideo.cpp
  timecode.cpp
  color_matrix.cpp
  logger.cpp
  ssim.cpp
)

target_include_directories(opensup_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_precompile_headers(opensup_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h
)

set_project_warnings(opensup_common)
