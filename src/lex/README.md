# src/lex

Clause 5 — bytes to tokens

The stream ends with EOF; line terminators survive as NEWLINE
tokens for the layout stage. A `///`
line yields one DOC token before its NEWLINE; plain `//` comments
are skipped.
