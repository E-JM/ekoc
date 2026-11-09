" Vim syntax file
" Language:     Eko
" Spec:         ~/Work/EkoDoc
" Filenames:    *.eko
"
" The surface is the one specified by the reference manual:
" 02-syntax.md    layout, comments, literals, operators
" 03-control.md   bindings, lambdas, case/if, patterns
" 04-types.md     type/ctype declarations, composites, generics
" 05-names.md     imports, UFCS calls, module paths
" 06-effects.md   world bit arrows (!-> ?->) and capabilities
" 10-prelude.md   prelude types, free functions, namespaces
" 11-ffi.md       extern declarations and the C vocabulary

if exists("b:current_syntax")
  finish
endif

" ── Reserved words (02-syntax.md §1.2) ────────────────────────────
" Only these four words are reserved. `type`, `ctype`, `test`, and
" `Pure` are slot words and are matched contextually below.
syn keyword ekoConditional if else case
syn keyword ekoInclude     import
syn keyword ekoBoolean     True False

" ── Types (02-syntax.md §5, 04-types.md, 10-prelude.md §3) ────────
" Prelude types and the machine words of the FFI boundary.
syn keyword ekoBuiltinType
  \ Bool Char String Bytes
  \ Int UInt Float
  \ I8 I16 I32 I64 U8 U16 U32 U64 F32 F64
  \ Range Option Result Set
  \ Time Mono Duration Json ParseError IoError Never
  \ File OpenMode Scope Task
" Capabilities (06-effects.md §2).
syn keyword ekoCapability Out In Fs Env Args Clock Rand Proc Conc
" Prelude constructors (08-builtins.md §1, §6; 06-effects.md §3).
" True and False stay in ekoBoolean above.
syn keyword ekoConstructor Some None Ok Err
" Everything PascalCase is a declared type, constructor, or namespace.
syn match ekoType '\v<_?[A-Z]\w*>'
" `C.Ptr.Void`, `C.Size`, and the audited borrow `C.In` (11-ffi.md §3).
syn match ekoBuiltinType '\v<C\.%(Ptr\.Void|Size|In)>'

" ── Calls and members (05-names.md §1–3) ──────────────────────────
" A lowercase name directly before `(` is a call or a definition.
syn match ekoFunctionCall '\v<\zs<[a-z_]\w*>\ze\s*\('
" A member without a call is a field or a function reference; a member
" before `(` is a call.  The call is defined last so it wins.
syn match ekoField      '\v(\.)\zs<[a-z_]\w*>'
syn match ekoMemberCall '\v(\.)\zs<[a-z_]\w*>\ze\s*\('
" The prelude's zero-type free functions are in unqualified scope.
" A dot before the name means it is a member, not the free function.
syn match ekoBuiltin '\%(\.\)\@<!\<\%(show\|inspect\|panic\|assert\|assert_eq\|assert_ne\)\>'

" ── Declarations and bindings (03-control.md §1, 04-types.md §1) ──
" `name := value` (rebinding) and `name: annotation = value` put the
" bound name in the value namespace.  These matches come after the
" generic type/call matches so a declaration name wins at its own
" position.
syn match ekoBinding '\v%(^\s*)\zs<[a-z_]\w*>\ze\s*:\='
syn match ekoBinding '^\s*\zs\<[a-z_]\w*\>\ze\s*<-'
" Tuple destructuring: `(a, b) = value` and `(p, i) <- call`.
syn match ekoBinding '\%(^\s*(\)\@<=[^)]*\ze)\s*=\%(=\)\@!'
syn match ekoBinding '\%(^\s*(\)\@<=[^)]*\ze)\s*<-'
syn match ekoBinding '\v%(^\s*)\zs<[a-z_]\w*>\ze\s*:'
" A `type` / `ctype` declaration names a type; a `test` declaration
" names a test; `name: (params)...` names a function.
syn match ekoTypeDeclName '\v%(^\s*)\zs<\h\w*>\ze\s*:\s*%(ctype|type)>'
syn match ekoTestName     '\v%(^\s*)\zs<\h\w*>\ze\s*:\s*test>'
syn match ekoFunctionDef  '\v%(^\s*)\zs<\h\w*>\ze\s*:\s*\('
" Namespace member declaration: `Type.member: ...` (05-names.md §8).
syn match ekoMemberDef    '\%(^\s*\<_*[A-Z]\w*\>\.\)\@<=\<[a-z]\w*\>\ze\s*:'

" ── Slot words (03-control.md §1, 04-types.md §1, 09 §6, 11 §2) ───
" `type` / `ctype` / `test` sit in the annotation slot after `:`;
" `extern` and `Pure` sit on the right-hand side of an extern binding.
syn match ekoSlotWord '\%(\<\h\w*\>\s*:\s*\)\@<=\%(ctype\|type\|test\)\>'
syn match ekoExtern   '\%(=[ \t]*\)\@<=extern\>'
syn match ekoPure     '\%(extern[ \t]\+"[^"]*"[ \t]\+\)\@<=Pure\>'

" ── Imports (05-names.md §6) ──────────────────────────────────────
" `import csv/lex`: the path locates the file; `/` is not division here.
syn match ekoModule '\%(^\s*import\s\+\)\@<=[a-z_]\w*\%(/[a-z_]\w*\)*'

" ── Wildcard (03-control.md §5, 06-effects.md §5.3) ───────────────
" `_` is the wildcard / unit / discard; a lone `_` is never an
" identifier because identifiers continue after it.
syn match ekoWildcard '\v<_\ze%(\W|$)'

" ── Literals (02-syntax.md §4, 08-builtins.md §2) ─────────────────
" Decimal, hexadecimal, and binary integers; floats have a decimal
" point or an exponent (02-syntax.md §4.1).  Underscores group digits.
syn match ekoInt   '\v<[0-9][0-9_]*>'
syn match ekoFloat '\v<[0-9][0-9_]*\.[0-9][0-9_]*%([eE][+-]?[0-9]+)?>'
syn match ekoFloat '\v<[0-9][0-9_]*[eE][+-]?[0-9]+>'
syn match ekoHex   '\v<0x[0-9a-fA-F_]+>'
syn match ekoBin   '\v<0b[01_]+>'

" Strings are single-line; interpolation slots are `{expr}`.  A slot
" opens only when the `{` is preceded by an even number of backslashes
" and is not the brace of a `\u{...}` escape, so `\{` is a literal
" brace and `\\{` opens a slot.
syn cluster ekoExpr contains=ekoString,ekoChar,ekoConditional,ekoBoolean,
  \ ekoBuiltin,ekoBuiltinType,ekoCapability,ekoConstructor,ekoType,
  \ ekoSlotWord,ekoFunctionCall,ekoMemberCall,ekoField,ekoInt,
  \ ekoFloat,ekoHex,ekoBin,ekoWildcard,ekoOperator,ekoPunct,
  \ ekoArrow,ekoWorldBit
syn region ekoString start=+"+ skip=+\\\\\|\\"\|\\u{[0-9a-fA-F]\+}+ end=+"+
  \ contains=ekoInterpolation
" A composite literal or pattern is a brace region, so nested braces
" are counted both at the top level and inside an interpolation slot.
syn region ekoBraces matchgroup=ekoPunct start=+{+ end=+}+
  \ contains=@ekoExpr,ekoBraces
syn region ekoInterpolation matchgroup=ekoInterpolationBrace
  \ start=+\%(\%([^\\]\|^\)\%(\\\\\)*\)\@<=\%(\%([^\\]\|^\)\%(\\\\\)*\\u\)\@<!{+ end=+}+ contained skip=+\\\\|\\"+
  \ contains=@ekoExpr,ekoBraces

" Character literals: same escapes as strings plus \' (02-syntax.md §4.2).
syn region ekoChar start=+'+ skip=+\\\\\|\\'+ end=+'+

" ── Operators (02-syntax.md §6, 06-effects.md §5.2) ───────────────
" Single-character operators first, then the longer tokens, then the
" arrows, so the longest spelling wins at a shared start position.
syn match ekoPunct    display '[()\[\],;]'
syn match ekoOperator display '[-+*/%<>=|]'
syn match ekoOperator display ':=\|&&\|||\|==\|!=\|<=\|>=\|++\|\.\.=\|\.\.'
syn match ekoArrow    display '!->\|?->\|->\|<-'
" `!` / `?` alone are the elided arrow forms after a parameter group.
syn match ekoWorldBit display '\%()\s*\)\@<=[!?]\ze\s*\%(=\s*$\|\s*$\)'

" ── Comments (02-syntax.md §3) ────────────────────────────────────
" Defined last so `//` wins over the division operator.  `///` is a
" documentation comment; `//` is an ordinary line comment.
syn match ekoComment    '//.*$'
syn match ekoDocComment '///.*$'

" Doc comments keep the comment colour and are italicised, so `///` stays
" distinguishable from `//` without colliding with Type: themes like
" tokyonight give Special (and therefore SpecialComment) the same fg as
" Type.  A `hi def link` cannot add the italic, so the group is built from
" Comment and re-applied after every colorscheme change (Omarchy hot-reloads
" themes with `hi clear`).
function! s:EkoDocCommentHl() abort
  let l:hl = nvim_get_hl(0, {'name': 'Comment', 'link': v:false})
  let l:hl.italic = v:true
  call nvim_set_hl(0, 'ekoDocComment', l:hl)
endfunction

augroup eko_doc_comment
  autocmd!
  autocmd ColorScheme * call s:EkoDocCommentHl()
  autocmd VimEnter     * call s:EkoDocCommentHl()
augroup END
call s:EkoDocCommentHl()

hi def link ekoComment          Comment
hi def link ekoConditional      Conditional
hi def link ekoInclude          Include
hi def link ekoBoolean          Boolean
hi def link ekoBuiltinType      Type
hi def link ekoCapability       Type
hi def link ekoConstructor      Type
hi def link ekoType             Type
hi def link ekoBuiltin          Function
hi def link ekoFunctionCall     Function
hi def link ekoField            Identifier
hi def link ekoMemberCall       Function
hi def link ekoBinding          Identifier
hi def link ekoTypeDeclName     Type
hi def link ekoTestName         Function
hi def link ekoFunctionDef      Function
hi def link ekoMemberDef        Function
hi def link ekoSlotWord         Keyword
hi def link ekoExtern           Keyword
hi def link ekoPure             Keyword
hi def link ekoModule           Include
hi def link ekoWildcard         Special
hi def link ekoInt              Number
hi def link ekoFloat            Float
hi def link ekoHex              Number
hi def link ekoBin              Number
hi def link ekoString           String
hi def link ekoInterpolationBrace Special
hi def link ekoChar             String
hi def link ekoPunct            Delimiter
hi def link ekoOperator         Operator
hi def link ekoArrow            Operator
hi def link ekoWorldBit         Special

let b:current_syntax = "eko"
