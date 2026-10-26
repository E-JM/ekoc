#include "parse/internal.h"

/* Type expressions of Clause 6.9 / A.3.7. Parentheses are
   context-sensitive: a group where a complete type is expected is a
   parameter group with the arrow possibly elided; where an atom is
   expected, it is a grouped type. */

static ek_node *fn_type(p_ctx *c, const ek_layout_token *open);
static ek_node *fn_params(p_ctx *c, ek_node *n);
static ek_node *fn_arrow(p_ctx *c, ek_node *n);
static ek_node *applied_type(p_ctx *c);
static ek_node *bracket_tail(p_ctx *c, const ek_layout_token *open,
                             ek_node *key);
static ek_node *bracket_type(p_ctx *c, const ek_layout_token *open);
static ek_node *tuple_type(p_ctx *c, const ek_layout_token *open,
                           ek_node *first);
static ek_node *record_type(p_ctx *c, const ek_layout_token *open);
static void arg_atom(p_ctx *c, ek_nlist *args);

ek_node *p_type(p_ctx *c)
{
    if (c->oom) {
        return NULL;
    }
    if (p_at(c, EK_TOK_LPAREN)) {
        return fn_type(c, p_peek(c));
    }
    return applied_type(c);
}

/* FunctionType := ParameterGroup ( '->' | '!->' | '?->' ) Type
   | ParameterGroup '!' | ParameterGroup '?' | ParameterGroup (A.3.7).
   The elided forms carry result NULL, meaning `_`. */
static ek_node *fn_type(p_ctx *c, const ek_layout_token *open)
{
    ek_node *n = p_node(c, EK_N_T_FN, open->line, open->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.t_fn.params);
    n->u.t_fn.result = NULL;
    n->u.t_fn.arrow = EK_ARROW_PURE;
    p_take(c); /* '(' */
    if (!fn_params(c, n)) {
        return n;
    }
    return fn_arrow(c, n);
}

/* [ Parameter { ',' Parameter } ] then ')'; breaks are free. */
static ek_node *fn_params(p_ctx *c, ek_node *n)
{
    p_skip_newlines(c);
    if (p_at(c, EK_TOK_RPAREN)) {
        p_take(c);
        return n;
    }
    for (;;) {
        const ek_layout_token *t = p_peek(c);
        const char *label = NULL;
        ek_node *ty;
        ek_node *param;

        if (p_src_kind(t) == EK_TOK_IDENT
            && p_src_kind(p_peek_at(c, 1)) == EK_TOK_COLON) {
            label = p_take(c)->tok.text;
            p_take(c);
        }
        ty = p_type(c);
        if (!ty) {
            return NULL;
        }
        param = p_node(c, EK_N_T_PARAM, t->line, t->col);
        if (param) {
            param->u.t_field.name = label;
            param->u.t_field.type = ty;
        }
        p_list_add(c, &n->u.t_fn.params, param);
        p_skip_newlines(c);
        if (p_got(c, EK_TOK_COMMA)) {
            p_skip_newlines(c);
            continue;
        }
        break;
    }
    if (!p_expect(c, EK_TOK_RPAREN, "')'")) {
        p_sync(c);
        return NULL;
    }
    return n;
}

/* The arrow forms of Clause 6.9.3; elided forms carry result NULL. */
static ek_node *fn_arrow(p_ctx *c, ek_node *n)
{
    if (p_got(c, EK_TOK_ARROW)) {
        n->u.t_fn.result = p_type(c);
    } else if (p_got(c, EK_TOK_BANGARROW)) {
        n->u.t_fn.arrow = EK_ARROW_WORLD;
        n->u.t_fn.result = p_type(c);
    } else if (p_got(c, EK_TOK_QUESTIONARROW)) {
        n->u.t_fn.arrow = EK_ARROW_OPEN;
        n->u.t_fn.result = p_type(c);
    } else if (p_got(c, EK_TOK_BANG)) {
        n->u.t_fn.arrow = EK_ARROW_WORLD;
    } else if (p_got(c, EK_TOK_QUESTION)) {
        n->u.t_fn.arrow = EK_ARROW_OPEN;
    }
    return n;
}

/* AppliedType := TypeAtom { TypeAtom }; a chain stops at `,` and `->`
   because neither begins an atom (Clause 6.9.4). */
static ek_node *applied_type(p_ctx *c)
{
    ek_node *head = p_type_atom(c);
    ek_node *n;

    if (!head) {
        return NULL;
    }
    if (!p_atom_start(p_src_kind(p_peek(c)))) {
        return head;
    }
    if (head->kind == EK_N_T_NAME) {
        size_t i;
        int named = head->u.path.count > 0;

        for (i = 0; i < head->u.path.count; i++) {
            if (!p_is_pascal(head->u.path.data[i])) {
                named = 0;
            }
        }
        if (!named) {
            p_error(c, "E-apply-head", head->line, head->col,
                    "a type variable must not head a generic application "
                    "(Clause 6.9.4)");
        }
    }
    n = p_node(c, EK_N_T_APPLY, head->line, head->col);
    if (!n) {
        return NULL;
    }
    n->u.t_apply.head = head;
    p_list_init(&n->u.t_apply.args);
    while (p_atom_start(p_src_kind(p_peek(c))) && !c->oom) {
        arg_atom(c, &n->u.t_apply.args);
    }
    return n;
}

/* An application argument is an arrow-free atom, and a composite
   argument is parenthesized (Clause 6.9.4). */
static void arg_atom(p_ctx *c, ek_nlist *args)
{
    ek_node *a = p_type_atom(c);

    if (a && (a->kind == EK_N_T_LIST || a->kind == EK_N_T_DICT
              || a->kind == EK_N_T_TUPLE || a->kind == EK_N_T_RECORD)) {
        p_error(c, "E-syntax", a->line, a->col,
                "a composite argument of a generic application is "
                "parenthesized (Clause 6.9.4)");
    }
    p_list_add(c, args, a);
}

/* TypeAtom of A.3.7: a (qualified) name, the unit type, a grouped
   type, or one of the bracket forms. */
ek_node *p_type_atom(p_ctx *c)
{
    const ek_layout_token *t = p_peek(c);

    switch (p_src_kind(t)) {
    case EK_TOK_IDENT: {
        ek_node *n = p_node(c, EK_N_T_NAME, t->line, t->col);

        p_take(c);
        if (n) {
            p_path_init(&n->u.path);
            p_path_add(c, &n->u.path, t->tok.text);
            while (p_at(c, EK_TOK_DOT)
                   && p_peek_at(c, 1)->kind == EK_LTOK_SOURCE
                   && p_peek_at(c, 1)->tok.kind == EK_TOK_IDENT) {
                p_take(c);
                p_path_add(c, &n->u.path, p_take(c)->tok.text);
            }
        }
        return n;
    }
    case EK_TOK_LPAREN: {
        ek_node *inner;
        ek_node *n;

        p_take(c);
        inner = p_type(c);
        if (!inner) {
            return NULL;
        }
        if (!p_expect(c, EK_TOK_RPAREN, "')'")) {
            return NULL;
        }
        n = p_node(c, EK_N_T_PAREN, t->line, t->col);
        if (n) {
            n->u.wrap.inner = inner;
        }
        return n;
    }
    case EK_TOK_LBRACKET:
        return bracket_type(c, t);
    default:
        p_error_tok(c, "E-syntax", t, "expected a type");
        return NULL;
    }
}

/* '[]' Type | '[' Type ']' Type | '[' Type { ',' Type } ']'
   | '[' Field { ',' Field } ']' (A.3.7). The bracket forms are told
   apart by what follows the first type: `]` plus an atom is a
   dictionary, `,` a tuple, `name :` a record (Clause 6.9.2). */
static ek_node *bracket_type(p_ctx *c, const ek_layout_token *open)
{
    ek_node *n;
    ek_node *first;

    p_take(c); /* '[' */
    p_skip_newlines(c);
    if (p_got(c, EK_TOK_RBRACKET)) {
        ek_node *element = p_type(c);

        if (!element) {
            return NULL;
        }
        n = p_node(c, EK_N_T_LIST, open->line, open->col);
        if (n) {
            n->u.wrap.inner = element;
        }
        return n;
    }
    if (p_src_kind(p_peek(c)) == EK_TOK_IDENT
        && p_src_kind(p_peek_at(c, 1)) == EK_TOK_COLON) {
        return record_type(c, open);
    }
    first = p_type(c);
    if (!first) {
        return NULL;
    }
    p_skip_newlines(c);
    if (p_got(c, EK_TOK_RBRACKET)) {
        return bracket_tail(c, open, first);
    }
    if (p_got(c, EK_TOK_COMMA)) {
        return tuple_type(c, open, first);
    }
    p_error_tok(c, "E-syntax", p_peek(c),
                "expected `]`, `,` or a field in the bracket type");
    p_sync(c);
    p_got(c, EK_TOK_RBRACKET);
    return NULL;
}

/* After `[ Type ]`: an atom begins the value of a dictionary; nothing
   is the invalid one-element bracket (Clause 6.9.2). */
static ek_node *bracket_tail(p_ctx *c, const ek_layout_token *open,
                             ek_node *key)
{
    ek_node *value;
    ek_node *n;

    if (!p_atom_start(p_src_kind(p_peek(c)))) {
        p_error(c, "E-syntax", open->line, open->col,
                "`[t]` is not a valid type; a tuple has at least two "
                "elements (Clause 6.9.2)");
        return NULL;
    }
    value = p_type(c);
    if (!value) {
        return NULL;
    }
    n = p_node(c, EK_N_T_DICT, open->line, open->col);
    if (n) {
        n->u.pair.a = key;
        n->u.pair.b = value;
    }
    return n;
}

/* TupleType := '[' Type ',' Type { ',' Type } ']' (A.3.7). */
static ek_node *tuple_type(p_ctx *c, const ek_layout_token *open,
                           ek_node *first)
{
    ek_node *n = p_node(c, EK_N_T_TUPLE, open->line, open->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.kids);
    p_list_add(c, &n->u.kids, first);
    for (;;) {
        ek_node *item = p_type(c);

        p_list_add(c, &n->u.kids, item);
        if (!item || c->oom) {
            return n;
        }
        p_skip_newlines(c);
        if (p_got(c, EK_TOK_COMMA)) {
            p_skip_newlines(c);
            continue;
        }
        if (p_got(c, EK_TOK_RBRACKET)) {
            return n;
        }
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `]` in the tuple type");
        p_sync(c);
        p_got(c, EK_TOK_RBRACKET);
        return n;
    }
}

/* RecordType := '[' Field { ',' Field } ']' (A.3.7). */
static ek_node *record_type(p_ctx *c, const ek_layout_token *open)
{
    ek_node *n = p_node(c, EK_N_T_RECORD, open->line, open->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.kids);
    for (;;) {
        const ek_layout_token *name = p_peek(c);
        ek_node *ty;
        ek_node *field;

        if (p_src_kind(name) != EK_TOK_IDENT
            || p_src_kind(p_peek_at(c, 1)) != EK_TOK_COLON) {
            p_error_tok(c, "E-syntax", name,
                        "expected a field in the record type (Clause 6.9.2)");
            p_sync(c);
            p_got(c, EK_TOK_RBRACKET);
            return n;
        }
        p_take(c); /* the field name */
        p_take(c); /* ':' */
        field = p_node(c, EK_N_T_FIELD, name->line, name->col);
        if (!field) {
            return NULL;
        }
        field->u.t_field.name = name->tok.text;
        ty = p_type(c);
        if (!ty) {
            return NULL;
        }
        field->u.t_field.type = ty;
        p_list_add(c, &n->u.kids, field);
        p_skip_newlines(c);
        if (p_got(c, EK_TOK_COMMA)) {
            p_skip_newlines(c);
            continue;
        }
        if (p_got(c, EK_TOK_RBRACKET)) {
            return n;
        }
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `]` in the record type");
        p_sync(c);
        p_got(c, EK_TOK_RBRACKET);
        return n;
    }
}
