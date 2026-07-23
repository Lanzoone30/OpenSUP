add_executable(opensup_test_core
  segments_test.cpp
)

target_link_libraries(opensup_test_core
  PRIVATE
    opensup_core
    GTest::gtest_main
)

target_include_directories(opensup_test_core
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

gtest_discover_tests(opensup_test_core)
