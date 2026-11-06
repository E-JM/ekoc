# src/lower

checked AST to C source text; self-TCE loop transform

ek_lower_with is a pure entry point, (input, mode, arena, diag) ->
output; it never aborts. The output is one NUL-terminated C99
translation unit in the arena: the runtime include, the retention
helper, the generated struct layer of the constructed and read
declared types, a forward declaration per emitted function, the
function bodies, and the entry shim of the mode. Only top-level
bindings transitively reachable from the mode's roots are emitted;
fn_count counts them. A construct the lowerer does not cover is
diagnosed with code "E-lower" and skipped, so a failed run still
yields the text of the parts that lowered.
