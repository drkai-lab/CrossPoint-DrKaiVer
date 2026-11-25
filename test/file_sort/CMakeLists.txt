add_executable(FileSortTest
  FileSortTest.cpp
  ${REPO_ROOT}/lib/FileSort/FileSort.cpp
  ${REPO_ROOT}/lib/FsHelpers/FsHelpers.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(FileSortTest PRIVATE
  ${REPO_ROOT}/test/fs_helpers/stubs
  ${REPO_ROOT}/lib/FileSort
  ${REPO_ROOT}/lib/FsHelpers
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(FileSortTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FileSortTest)
