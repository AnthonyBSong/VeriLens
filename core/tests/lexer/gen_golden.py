#!/usr/bin/env python3
"""Generate .json golden files for Verilog/SV lexer tests.

Scans core/tests/examples/ for .v and .sv files and writes
<filename>.json golden files into this directory (core/tests/lexer/).
"""

import json, sys
from pathlib import Path

KEYWORD_TYPES = {
    "module": "MODULE", "endmodule": "ENDMODULE",
    "input": "INPUT", "output": "OUTPUT", "inout": "INOUT",
    "wire": "WIRE", "tri": "TRI", "tri0": "TRI0", "tri1": "TRI1",
    "wand": "WAND", "wor": "WOR", "supply0": "SUPPLY0", "supply1": "SUPPLY1",
    "reg": "REG", "logic": "LOGIC", "integer": "INTEGER", "real": "REAL",
    "time": "TIME", "realtime": "REALTIME",
    "parameter": "PARAMETER", "localparam": "LOCALPARAM", "defparam": "DEFPARAM",
    "always": "ALWAYS", "always_ff": "ALWAYS_FF", "always_comb": "ALWAYS_COMB",
    "always_latch": "ALWAYS_LATCH",
    "initial": "INITIAL", "begin": "BEGIN", "end": "END",
    "if": "IF", "else": "ELSE", "case": "CASE", "casex": "CASEX",
    "casez": "CASEZ", "endcase": "ENDCASE", "default": "DEFAULT",
    "for": "FOR", "while": "WHILE", "repeat": "REPEAT", "forever": "FOREVER",
    "disable": "DISABLE", "fork": "FORK", "join": "JOIN",
    "posedge": "POSEDGE", "negedge": "NEGEDGE",
    "assign": "ASSIGN", "deassign": "DEASSIGN", "force": "FORCE", "release": "RELEASE",
    "and": "AND", "or": "OR", "not": "NOT", "nand": "NAND", "nor": "NOR",
    "xor": "XOR", "xnor": "XNOR", "buf": "BUF",
    "bufif0": "BUFIF0", "bufif1": "BUFIF1", "notif0": "NOTIF0", "notif1": "NOTIF1",
    "function": "FUNCTION", "endfunction": "ENDFUNCTION",
    "task": "TASK", "endtask": "ENDTASK", "automatic": "AUTOMATIC", "return": "RETURN",
    "generate": "GENERATE", "endgenerate": "ENDGENERATE", "genvar": "GENVAR",
    "strong0": "STRONG0", "strong1": "STRONG1", "weak0": "WEAK0", "weak1": "WEAK1",
    "highz0": "HIGHZ0", "highz1": "HIGHZ1", "pull0": "PULL0", "pull1": "PULL1",
    "signed": "SIGNED", "unsigned": "UNSIGNED",
    "specify": "SPECIFY", "endspecify": "ENDSPECIFY", "specparam": "SPECPARAM",
    "typedef": "TYPEDEF", "enum": "ENUM", "struct": "STRUCT", "union": "UNION",
    "packed": "PACKED",
}

THREE_MAP = {
    "===": "EQ_EQ_EQ", "!==": "BANG_EQ_EQ",
    "<<<": "LESS_LESS_LESS", ">>>": "GREATER_GREATER_GREATER",
}
TWO_MAP = {
    "==": "EQ_EQ", "!=": "BANG_EQ", "<=": "LESS_EQ", ">=": "GREATER_EQ",
    "<<": "LESS_LESS", ">>": "GREATER_GREATER", "**": "STAR_STAR",
    "&&": "AMP_AMP", "||": "PIPE_PIPE",
    "~&": "TILDE_AMP", "~|": "TILDE_PIPE", "~^": "TILDE_CARET", "^~": "TILDE_CARET",
    "::": "COLON_COLON",
}
ONE_MAP = {
    '+': "PLUS", '-': "MINUS", '*': "STAR", '/': "SLASH", '%': "PERCENT",
    '^': "CARET", '!': "BANG", '&': "AMP", '|': "PIPE", '~': "TILDE",
    '<': "LESS", '>': "GREATER", '=': "EQ", '?': "QUESTION",
    ':': "COLON", ';': "SEMICOLON", ',': "COMMA", '.': "DOT",
    '#': "HASH", '@': "AT", "'": "APOSTROPHE",
    '(': "LPAREN", ')': "RPAREN",
    '[': "LBRACKET", ']': "RBRACKET",
    '{': "LBRACE", '}': "RBRACE",
}

def tokenize(src):
    tokens = []
    i, n = 0, len(src)

    while i < n:
        if src[i].isspace():
            i += 1; continue
        if src[i:i+2] == '//':
            while i < n and src[i] != '\n': i += 1
            continue
        if src[i:i+2] == '/*':
            i += 2
            while i < n and src[i:i+2] != '*/': i += 1
            i += 2; continue
        if src[i] == '`':
            j = i + 1
            while j < n and (src[j].isalnum() or src[j] == '_'): j += 1
            tokens.append({"type": "COMPILER_DIRECTIVE", "lexeme": src[i:j]})
            i = j; continue
        if src[i] == '$':
            j = i + 1
            while j < n and (src[j].isalnum() or src[j] == '_'): j += 1
            tokens.append({"type": "SYSTEM_TASK", "lexeme": src[i:j]})
            i = j; continue
        if src[i] == '"':
            j = i + 1
            while j < n and src[j] != '"':
                if src[j] == '\\': j += 1
                j += 1
            j += 1
            tokens.append({"type": "STRING_LITERAL", "lexeme": src[i:j]})
            i = j; continue
        if src[i].isdigit():
            j = i
            while j < n and (src[j].isdigit() or src[j] == '_'): j += 1
            # Sized literal — optional signed marker:  4'b1010, 8'sh1F, 32'sd-1
            if j < n and src[j] == "'":
                signed_lit = (j+1 < n and src[j+1].lower() == 's')
                base_off = 2 if signed_lit else 1
                if j+base_off < n and src[j+base_off].lower() in 'bodh':
                    k = j + base_off + 1
                    while k < n and (src[k].isalnum() or src[k] == '_'): k += 1
                    tokens.append({"type": "INTEGER_LITERAL", "lexeme": src[i:k]})
                    i = k; continue
            # Real with fraction
            if j < n and src[j] == '.' and j+1 < n and src[j+1].isdigit():
                j += 1
                while j < n and (src[j].isdigit() or src[j] == '_'): j += 1
                if j < n and src[j] in 'eE':
                    j += 1
                    if j < n and src[j] in '+-': j += 1
                    while j < n and src[j].isdigit(): j += 1
                tokens.append({"type": "REAL_LITERAL", "lexeme": src[i:j]})
                i = j; continue
            # Real with bare exponent: 1e5, 1E+10
            if j < n and src[j] in 'eE':
                k = j + 1
                if k < n and src[k] in '+-': k += 1
                if k < n and src[k].isdigit():
                    j = k + 1
                    while j < n and src[j].isdigit(): j += 1
                    tokens.append({"type": "REAL_LITERAL", "lexeme": src[i:j]})
                    i = j; continue
            tokens.append({"type": "INTEGER_LITERAL", "lexeme": src[i:j]})
            i = j; continue
        if src[i].isalpha() or src[i] == '_':
            j = i
            while j < n and (src[j].isalnum() or src[j] == '_'): j += 1
            word = src[i:j]
            tokens.append({"type": KEYWORD_TYPES.get(word, "IDENTIFIER"), "lexeme": word})
            i = j; continue
        # Unsized literal: 'b1010, 'h1F, '0, '1, 'x, 'z
        if src[i] == "'" and i + 1 < n and (
            src[i+1].lower() in "bodhxz" or src[i+1] in "01"
        ):
            j = i + 2
            while j < n and (src[j].isalnum() or src[j] == '_'): j += 1
            tokens.append({"type": "INTEGER_LITERAL", "lexeme": src[i:j]})
            i = j; continue
        if src[i:i+3] in THREE_MAP:
            tokens.append({"type": THREE_MAP[src[i:i+3]], "lexeme": src[i:i+3]}); i += 3
        elif src[i:i+2] in TWO_MAP:
            tokens.append({"type": TWO_MAP[src[i:i+2]], "lexeme": src[i:i+2]}); i += 2
        elif src[i] in ONE_MAP:
            tokens.append({"type": ONE_MAP[src[i]], "lexeme": src[i]}); i += 1
        else:
            tokens.append({"type": "UNKNOWN", "lexeme": src[i]}); i += 1

    tokens.append({"type": "END_OF_FILE", "lexeme": ""})
    return tokens


if __name__ == "__main__":
    lexer_dir   = Path(__file__).resolve().parent
    examples_dir = lexer_dir.parent / "examples"

    files = sorted(examples_dir.rglob("*.v")) + sorted(examples_dir.rglob("*.sv"))
    if not files:
        print(f"No .v/.sv files found in {examples_dir}")
        sys.exit(1)

    for vfile in files:
        src    = vfile.read_text()
        tokens = tokenize(src)
        out    = lexer_dir / (vfile.name + ".json")
        out.write_text(json.dumps(tokens, indent=2))
        print(f"  {vfile.name} → {out.name}  ({len(tokens)} tokens)")
