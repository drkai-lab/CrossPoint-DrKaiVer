add_executable(OsBackupTest
  OsBackupTest.cpp
  ${REPO_ROOT}/lib/OsBackup/OsBackup.cpp
)

target_include_directories(OsBackupTest PRIVATE
  ${REPO_ROOT}/lib/OsBackup
)

target_link_libraries(OsBackupTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(OsBackupTest)
