add_executable(FileCacheTest
  FileCacheTest.cpp
  ${REPO_ROOT}/lib/FileCache/FileCache.cpp
)

target_include_directories(FileCacheTest PRIVATE
  ${REPO_ROOT}/lib/FileCache
)

target_link_libraries(FileCacheTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FileCacheTest)
