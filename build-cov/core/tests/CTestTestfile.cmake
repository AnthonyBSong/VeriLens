# CMake generated Testfile for 
# Source directory: /Users/song/Projects/VeriLens/core/tests
# Build directory: /Users/song/Projects/VeriLens/build-cov/core/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/Users/song/Projects/VeriLens/build-cov/core/tests/lexer_tests_e3b0c442_include.cmake")
include("/Users/song/Projects/VeriLens/build-cov/core/tests/parser_tests_e3b0c442_include.cmake")
include("/Users/song/Projects/VeriLens/build-cov/core/tests/validator_tests_e3b0c442_include.cmake")
add_test("gen_ast_examples" "sh" "-c" [[/Users/song/Projects/VeriLens/build-cov/core/tools/gen_ast --no-validate /Users/song/Projects/VeriLens/core/tests/examples > /dev/null; test $? -le 3]])
set_tests_properties("gen_ast_examples" PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/Users/song/Projects/VeriLens/core/tests/CMakeLists.txt;42;add_test;/Users/song/Projects/VeriLens/core/tests/CMakeLists.txt;0;")
