#include <gtest/gtest.h>
#include "Lexer.h"

std::string src = R"(
module counter (
    input clk,
    output reg [7:0] q
);
endmodule
)";

TEST(LexerTest, BasicModule) {
    Lexer lexer("module foo; endmodule");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::MODULE);
    EXPECT_EQ(tokens[1].lexeme, "foo");
}

TEST(LexerTest, SizedLiteral) {
    Lexer lexer("4'b1010");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::INTEGER_LITERAL);
    EXPECT_EQ(tokens[0].lexeme, "4'b1010");
}