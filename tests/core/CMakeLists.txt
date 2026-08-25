add_executable(opensup_test_core
  pgstream_test.cpp
  threads_test.cpp
  ssim_test.cpp
  extra_acq_test.cpp
  refresh_acq_test.cpp
  group_chain_test.cpp
  redraw_test.cpp
  layout_engine_test.cpp
  params_test.cpp
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

target_compile_definitions(opensup_test_core
  PRIVATE
    OPENDSUP_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures"
)

gtest_discover_tests(opensup_test_core)
