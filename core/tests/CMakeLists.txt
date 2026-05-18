find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(lexer_tests
    lexer/basic_test.cpp
    ../Lexer.cpp
)

target_include_directories(lexer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(lexer_tests PRIVATE GTest::gtest GTest::gtest_main)

gtest_discover_tests(lexer_tests)
