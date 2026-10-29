# 5. Lexical Structure

## 5.1 Source Text

### 5.1.1 Encoding

An Eko source file has the extension `.eko`. Its content is a sequence of
Unicode scalar values encoded in UTF-8 (RFC 3629). The encoding MUST be valid
UTF-8; an invalid byte sequence is a lexical error.

A byte-order mark (U+FEFF) at the start of the file is ignored. A U+FEFF at
any other position is an error.

### 5.1.2 Line Terminators

A **line terminator** is U+000A (LINE FEED). A U+000D (CARRIAGE RETURN)
immediately preceding a line terminator is ignored. A U+000D at any other
position is an error.

A source file MAY end with or without a final line terminator. The end of the
file is treated as a line terminator for layout purposes.

### 5.1.3 End of File

The end of the file terminates the last line and closes every block that is
still open. A block that cannot be closed consistently with the layout rules
of Clause 5.2 is an error.

## 5.2 Layout and Indentation

### 5.2.1 Indentation Levels

One **indentation level** is exactly four U+0020 SPACE characters. Indentation
is significant: it determines block membership.

A line's **indentation** is the sequence of spaces at its start. Two lines are
at the same indentation level when they have the same number of leading
spaces.

### 5.2.2 Block Structure

A block is introduced by a header line and consists of the lines that follow
it at one indentation level deeper than the header. The first member line
establishes the block's column. Every member of the block begins in that
column. A nested block begins one or more levels to the right of its header.

The indentation of a member, relative to the enclosing block, MUST be a
multiple of four spaces. Any other relative indentation is an error.

Blank lines and lines that contain only a comment do not establish a block
column and do not terminate a block.

### 5.2.3 Tabs

U+0009 (CHARACTER TABULATION) is not valid whitespace in indentation or
between tokens. Its occurrence there is an error with code `E-tab` (Annex B).

A tab inside a comment is ignored. A tab inside a string or character literal
is written with the escape `\t` (Clause 5.9.2).

### 5.2.4 Blank Lines

Blank lines and comment-only lines are ignored for layout. They MAY appear
anywhere a line terminator is permitted.

## 5.3 Tokens and Separators

### 5.3.1 Token Classes

The following token classes exist:

| Class | Examples |
|---|---|
| identifier | `person`, `Person`, `_tmp` |
| keyword | `import`, `case`, `if`, `else` |
| literal | `42`, `1.5`, `'a'`, `"text"` |
| operator | `+`, `==`, `&&`, `++`, `..=` |
| punctuation | `(`, `)`, `[`, `]`, `{`, `}`, `,`, `:`, `.` |
| line terminator | U+000A |
| semicolon | `;` |
| end of file | the end of the source text |

The contextual words `type`, `ctype`, `test`, `extern`, and `Pure` are
lexically identifiers. They acquire their special meaning only in the
positions specified in Clause 5.6.2.

### 5.3.2 Semicolons

A semicolon `;` separates the items of a single-line suite. It terminates the
current inline item — an expression or a `case` arm — as a line terminator
terminates a member. It does not change the indentation stack and does not
open or close a block (Annex A.4). Whether the enclosing construct continues
after the semicolon is decided by that construct, exactly as for a line
terminator.

### 5.3.3 Newlines

A line terminator ends a block member. It does not end a member where a
continuation is licensed by Clause 6.2.4. A line terminator inside a bracket
group is free formatting.
