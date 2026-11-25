add_executable(PdfTextTest
  PdfTextTest.cpp
  ${REPO_ROOT}/lib/Pdf/PdfText.cpp
)

target_include_directories(PdfTextTest PRIVATE
  ${REPO_ROOT}/lib/Pdf
)

target_link_libraries(PdfTextTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PdfTextTest)
