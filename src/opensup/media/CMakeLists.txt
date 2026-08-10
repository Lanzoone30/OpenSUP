add_library(opensup_media
  palette.cpp
  pgraphics.cpp
  pgstream.cpp
  optimizer.cpp
  hextree_impl.cpp
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

# Ported brule HexTree core is C-style C++ with old-style casts; keep the
# MIT source faithful instead of rewriting every cast.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(hextree_impl.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-sign-conversion;-Wno-conversion;-Wno-shadow")
endif()
