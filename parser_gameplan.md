# Parser Gameplan

## Phase 1 — Extend the AST for richer analysis

Before writing any parser code, the AST needs to be richer to support FSM detection.
Right now `AlwaysBlock` is opaque; we need to look inside it.

**New/updated nodes to define:**

- [ ] `PortWidth` struct — replace raw `width` string with structured `{ int msb, lsb }` (needed for validator width checking)
- [ ] `Parameter` struct — replace raw strings with `{ string name, string default_value }`
- [x] `Expression` hierarchy — `IdentifierExpr`, `LiteralExpr`, `UnaryOpExpr`, `BinaryOpExpr`, `ConditionalExpr`, `ConcatExpr`, `ReplicationExpr`, `BitSelectExpr`, `PartSelectExpr` (see `ASTNodes/Expression.h`)
- [x] Update `Assign` — `lhs` and `rhs` are now `ExprPtr` instead of raw strings
- [ ] `Statement` base class — base for everything inside an always block
- [ ] `SeqBlock : Statement` — a `begin / end` block containing a list of statements
- [ ] `NonBlockingAssign : Statement` — `{ ExprPtr lhs, ExprPtr rhs }` (sequential logic, key for FSM detection)
- [ ] `BlockingAssign : Statement` — `{ ExprPtr lhs, ExprPtr rhs }` (combinational logic)
- [ ] `IfStatement : Statement` — `{ ExprPtr condition, Statement* then_branch, Statement* else_branch }`
- [ ] `CaseStatement : Statement` — `{ ExprPtr expr, vector<CaseItem> items }` (FSM state machine)
- [ ] `CaseItem` — `{ vector<ExprPtr> patterns, Statement* body }` (default case has empty patterns)
- [ ] Update `AlwaysBlock` — replace opaque body with `unique_ptr<Statement> body`
- [ ] Update `Module` — replace raw `vector<string> parameters` with `vector<Parameter>`

---

## Phase 2 — Parser class skeleton

```cpp
class Parser {
public:
    Parser(const std::vector<Token>& tokens);

    // --- Pass 1 ---
    // Scan tokens for `module <name>` and populate module_names_
    std::unordered_set<std::string> collectModuleNames();

    // --- Pass 2 ---
    // Entry point — returns all modules found in the token stream
    std::vector<Module> parse();

private:
    // --- Token stream helpers ---
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    Token consume();
    Token expect(TokenType type);       // consume or throw parse error
    bool  check(TokenType type) const;
    bool  match(TokenType type);        // consume if matches, else false

    // --- Error recovery ---
    void skipToSemicolon();             // panic: skip to next `;`
    void skipToEndmodule();             // panic: skip to `endmodule`
    void skipBlock();                   // skip a matched begin/end block

    // --- Top level ---
    Module parseModule();

    // --- Module header ---
    void parseModuleHeader(Module& mod);        // name + #() + port list
    void parseParameterList(Module& mod);       // #( parameter ... )
    void parsePortList(Module& mod);            // (a, b, c) or ANSI style

    // --- Module body items ---
    Port        parsePortDeclaration();         // input/output/inout ...
    NetDecl     parseNetDeclaration();          // wire/reg/logic ...
    Parameter   parseParameterDeclaration();    // parameter / localparam
    Instance    parseInstance();                // ModuleName #() instName ()
    Assign      parseContinuousAssign();        // assign lhs = rhs;
    AlwaysBlock parseAlwaysBlock();             // always @(...) ...
    // GenerateBlock parseGenerateBlock();      // future

    // --- Always block internals (needed for FSM) ---
    std::string                  parseSensitivityList();    // @(posedge clk, ...)
    std::unique_ptr<Statement>   parseStatement();          // dispatch to below
    std::unique_ptr<Statement>   parseSeqBlock();           // begin ... end
    std::unique_ptr<Statement>   parseIfStatement();        // if (...) ... else ...
    std::unique_ptr<Statement>   parseCaseStatement();      // case/casex/casez
    std::unique_ptr<CaseItem>    parseCaseItem();           // pattern: statement
    std::unique_ptr<Statement>   parseNonBlockingAssign(const std::string& lhs); // lhs <= rhs
    std::unique_ptr<Statement>   parseBlockingAssign(const std::string& lhs);    // lhs = rhs

    // --- Shared expression helpers ---
    std::string parseExpression();   // raw text capture for rhs/conditions
    std::string parseLValue();       // lhs of assignments (may include [sel])
    PortWidth   parseWidth();        // [msb:lsb]

    // --- State ---
    std::vector<Token>              tokens_;
    size_t                          pos_;
    std::unordered_set<std::string> module_names_;  // populated in pass 1
};
```

---

## Phase 3 — Implementation order

**Token helpers first** — everything depends on these:
- [ ] `current()`, `peek()`, `consume()`, `expect()`, `check()`, `match()`
- [ ] `skipToSemicolon()`, `skipBlock()`, `skipToEndmodule()`

**Pass 1:**
- [ ] `collectModuleNames()` — scan for `MODULE IDENTIFIER` pairs, ~10 lines

**Pass 2 top-down:**
- [ ] `parse()` — loop calling `parseModule()` until EOF
- [ ] `parseModule()` — calls header then dispatches body items by keyword
- [ ] `parseModuleHeader()` + `parseParameterList()` + `parsePortList()`
- [ ] `parsePortDeclaration()`, `parseNetDeclaration()`, `parseParameterDeclaration()`
- [ ] `parseContinuousAssign()`
- [ ] `parseInstance()` — uses `module_names_` to disambiguate from net declarations
- [ ] `parseAlwaysBlock()` + `parseSensitivityList()`
- [ ] `parseStatement()` dispatcher
- [ ] `parseSeqBlock()`, `parseIfStatement()`
- [ ] `parseCaseStatement()` + `parseCaseItem()` — FSM-critical path
- [ ] `parseBlockingAssign()`, `parseNonBlockingAssign()`

---

## Key design notes

### `parseStatement()` dispatcher

Looks at `current()` to decide which parse function to call:

| Current token | Calls |
|---|---|
| `BEGIN` | `parseSeqBlock()` |
| `IF` | `parseIfStatement()` |
| `CASE` / `CASEX` / `CASEZ` | `parseCaseStatement()` |
| `IDENTIFIER` | `parseLValue()` then check `=` vs `<=` |

### `parseInstance()` vs net declaration

This is why pass 1 matters. When you see `IDENTIFIER IDENTIFIER`, check if the
first name is in `module_names_`. If yes → instance. If no → net with implicit
type (common in older Verilog).

### `parseExpression()` can be lazy

For rhs values, conditions in `if`, and `case` expressions, just capture raw
token text until hitting `;`, `)`, or `:`. No need for a full expression parser
unless we later want constant folding for parameter resolution.
