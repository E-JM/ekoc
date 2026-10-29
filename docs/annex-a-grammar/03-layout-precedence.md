# Annex A. Grammar

## A.4 Layout Algorithm

The layout algorithm runs after lexical analysis and before syntactic
analysis. It inserts `NEWLINE`, `INDENT`, and `DEDENT` tokens. The licensed
continuation positions are exactly those of Clause 6.2.4; every other line
break ends a member.

1. **Physical lines.** Split the source at line terminators. Compute each
   line's indentation as its leading spaces. A tab in the indentation is an
   error (`E-tab`).
2. **Significant lines.** Discard blank lines and lines whose first non-space
   characters are `//` or `///`. They do not affect the indentation stack.
3. **Bracket suppression.** Bracket depth is the number of `(`, `[`, and `{`
   open at the end of a line. A line break at depth greater than zero emits
   no token, and the next line's indentation is ignored — except after a
   callback header (rule 4).
4. **Callback blocks.** A line whose final bracket depth is greater than
   zero and whose last tokens are a parameter group followed by `:` — a
   lambda or trailing-callback header (Clause 6.2.5) — opens a block. A
   line whose closing parameter group and colon return the depth to zero
   is not a callback header; it is a depth-zero line, and its colon opens
   a block under rule 6 (the case-arm colon of Clause 6.7.1). The block
   consists of the following lines whose indentation is greater than that
   of the header line. Before its first member the algorithm emits
   `NEWLINE INDENT`; the first member fixes the block's column exactly as
   rule 5 fixes a depth-zero block's (Clause 5.2.2), every later member of
   the block MUST equal it, and every member ends with a `NEWLINE` token.
   At a line indented at or left of the header's column the algorithm
   emits the pending `DEDENT`s. The closing bracket of the call closes any
   callback block still open and emits its `DEDENT`s.
5. **Indentation stack.** For a line at bracket depth zero with column `c`,
   against the stack of block columns, initially `[0]`:
   - `c` equals the top: emit `NEWLINE`.
   - `c` is greater: the step `c - top` MUST be a positive multiple of four;
     push `c` and emit `NEWLINE INDENT`. The first member of a block fixes
     its column, and every later line of that block MUST equal it.
   - `c` is smaller: pop until the top equals `c`, emitting one `DEDENT` per
     pop; if no stack entry equals `c`, the line is an error.

   `NEWLINE` is a terminator, not a separator: the token of this rule ends
   the previous significant line, so every significant depth-zero line ends
   with a `NEWLINE` marker, as the productions of Annex A.3 write it
   (`Arm`, `VariantLine`, `ImportLine`). An `INDENT` follows the header
   line's `NEWLINE`; the `DEDENT`s that close a block follow the last
   member's `NEWLINE`, precede the closing bracket of a callback block
   (rule 4), and precede `EOF` (rule 10).

   The first significant line opens no block, whatever its indentation: the
   stack remains `[0]`, the line's indentation is ignored, and a line the
   grammar rejects there is diagnosed at parse. An indented `else` as the
   first significant line is an error (rule 8).
6. **Headers and continuation.** A line at depth zero that ends in `:`, `=`,
   or `->` opens a block (Clause 6.2.3); a deeper following line is the
   block's first member and is processed by rule 5. The block-opening tokens
   are `:`, `=`, and `->`; a line that ends in `:=` opens no block, and a
   multi-line rebinding right-hand side is a continued expression under Clause
   6.2.4. The `->` opener is the `case` arm header: the block it opens is the
   arm body, whose last member is the arm's value. A deeper line after a line
   that opens no block is a continuation: it emits no token, and its
   indentation is ignored. Continuation is licensed by Clause 6.2.4; a deeper
   line in any other position is an error.
7. **Chain continuation.** A line whose first token is `.` and whose column
   equals the column of the first `.` of the chain continues the expression.
   No `NEWLINE` is emitted at the end of the previous line.
8. **else.** An `else` line at the same column as the `if` it matches
   continues that `if`; it does not begin a new member. The algorithm emits
   the `DEDENT`s required by the stack, and the parser reattaches `else`.
   The rule holds whether the branches are inline or block form. An `else`
   line whose column matches no open `if` header matches nothing and is an
   error.
9. **Semicolons.** `;` terminates the current inline item (Clause 5.3.2). It
   emits no layout marker — the source token passes through to the parser —
   does not change the indentation stack, and does not open or close a
   block.
10. **End of file.** Emit one `DEDENT` for every open level, then `EOF`.

## A.5 Precedence and Associativity

The precedence table of Clause 6.5.5 is repeated here for reference.

| Level | Operators | Associativity |
|---|---|---|
| 1 | postfix `.f()`, constructor juxtaposition | left |
| 2 | unary `-` | none |
| 3 | `* / %` | left |
| 4 | `+ - ++` | left |
| 5 | range `.. ..=` | none |
| 6 | `== != < <= > >=` | none |
| 7 | `&&` | left |
| 8 | `\|\|` | left |

A range expression at level 5 is not a value constructor in the same sense as
the other levels; it produces a `Range` value. A chain over a range requires
parentheses (Clause 6.5.6).

## A.6 Excluded Productions

The grammar contains no production for:

- a loop (`while`, `for`, `do`, `loop`);
- `break` or `continue`;
- a user-defined operator declaration;
- a type alias;
- a bitwise or exponentiation operator;
- an index expression `x[i]`;
- a dictionary pattern or an as-pattern;
- a multiline string literal;
- a block comment;
- a ternary expression.

Their absence from the grammar is their specification. Clause 6.10 gives the
normative prohibition for the syntactic forms; Clause 5.11 gives it for the
lexical ones.
