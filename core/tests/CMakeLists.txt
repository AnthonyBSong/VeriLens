find_package(GTest REQUIRED HINTS /opt/homebrew/lib/cmake/GTest)
find_package(nlohmann_json REQUIRED)
include(GoogleTest)

set(CORE_SOURCES
    ../Lexer.cpp
    ../Preprocessor.cpp
    ../Parser.cpp
    ../Linker.cpp
    ../Validator.cpp
)

# lexer tests
add_executable(lexer_tests
    lexer/lexer_test.cpp
    ../Lexer.cpp
)
target_include_directories(lexer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(lexer_tests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)
gtest_discover_tests(lexer_tests)

# parser tests
add_executable(parser_tests
    parser/parser_test.cpp
    ${CORE_SOURCES}
)
target_include_directories(parser_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(parser_tests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)
gtest_discover_tests(parser_tests)

# validator tests
add_executable(validator_tests
    validator/validator_test.cpp
    ${CORE_SOURCES}
)
target_include_directories(validator_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(validator_tests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)
gtest_discover_tests(validator_tests)

