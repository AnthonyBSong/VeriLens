#pragma once
#include <vector>
#include "Tokens/Token.h"

// Token-level preprocessor, run by the Parser constructor on the lexer output:
//   `define NAME [(args)] body   (body ends at end of line; `\` continues it)
//   `undef, `ifdef, `ifndef, `elsif, `else, `endif   (evaluated; inactive tokens dropped)
//   `NAME [(args)]                (expanded, nested expansion allowed)
//   `include, `timescale, `default_nettype, ... (dropped with the rest of their line)
// A macro that was never defined stays in the stream as a COMPILER_DIRECTIVE token.
std::vector<Token> preprocess(const std::vector<Token>& in);
