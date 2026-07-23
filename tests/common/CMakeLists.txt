add_executable(opensup_test_common
  geometry_test.cpp
  timecode_test.cpp
  bdvideo_test.cpp
  color_matrix_test.cpp
  memory_test.cpp
)

target_link_libraries(opensup_test_common
  PRIVATE
    opensup_common
    GTest::gtest_main
)

target_include_directories(opensup_test_common
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

gtest_discover_tests(opensup_test_common)
