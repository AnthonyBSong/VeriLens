add_executable(gen_ast gen_ast.cpp ../Lexer.cpp ../Preprocessor.cpp ../Parser.cpp ../Linker.cpp ../Validator.cpp)

target_include_directories(gen_ast PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

find_package(nlohmann_json REQUIRED)
target_link_libraries(gen_ast PRIVATE nlohmann_json::nlohmann_json)
