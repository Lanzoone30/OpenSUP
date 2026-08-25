add_library(opensup_core
  segments.cpp
  filestreams.cpp
  renderer.cpp
  interface.cpp
  pgstream.cpp
  layout_engine.cpp
  ctu.cpp
)

target_include_directories(opensup_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${PUGIXML_INCLUDE_DIRS}
    ${LIBIMAGEQUANT_INCLUDE_DIRS}
)

target_include_directories(opensup_core
  PRIVATE
    ${CMAKE_SOURCE_DIR}/extern
)

target_link_libraries(opensup_core
  PUBLIC
    opensup_common
    opensup_media
    ${PUGIXML_LIBRARIES}
    ${LIBIMAGEQUANT_LIBRARIES}
)

target_precompile_headers(opensup_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h
)

set_project_warnings(opensup_core)
