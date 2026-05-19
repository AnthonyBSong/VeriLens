find_package(GTest REQUIRED PATHS /opt/homebrew/lib/cmake/GTest NO_DEFAULT_PATH)
find_package(nlohmann_json REQUIRED)
include(GoogleTest)

add_executable(lexer_tests
    lexer/lexer_test.cpp
    ../Lexer.cpp
)

target_include_directories(lexer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(lexer_tests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)

gtest_discover_tests(lexer_tests)
