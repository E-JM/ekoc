# Compiler pipeline: stage sources and archives, the ekoc binary, and the
# shared compile rule for src sources. Included by the root Makefile, in
# order before rt/makefile and tests/makefile — not runnable standalone.
ifeq ($(strip $(BUILD)),)
$(error run make at the repository root; src/makefile is an include fragment)
endif

# Stage objects are wired in as they land (ARCHITECTURE.md contract):
#   support -> lex -> layout -> parse -> resolve -> check -> lower -> emit -> cli
SUPPORT_SRC := \
	src/support/arena.c \
	src/support/buf.c \
	src/support/diag.c \
	src/support/surface.c \
	src/support/utf8.c \
	src/support/vocab.c \
	src/support/xid.c \
	src/support/xid_tables.c

LEX_SRC := \
	src/lex/charlit.c \
	src/lex/escape.c \
	src/lex/ident.c \
	src/lex/lex.c \
	src/lex/number.c \
	src/lex/operator.c \
	src/lex/string.c

LAYOUT_SRC := \
	src/layout/layout.c \
	src/layout/process.c

PARSE_SRC := \
	src/parse/binding.c \
	src/parse/ctl.c \
	src/parse/decl.c \
	src/parse/expr.c \
	src/parse/literal.c \
	src/parse/parse.c \
	src/parse/pattern.c \
	src/parse/postfix.c \
	src/parse/type.c \
	src/parse/util.c

RESOLVE_SRC := \
	src/resolve/body.c \
	src/resolve/cap_source.c \
	src/resolve/chain.c \
	src/resolve/embedded.c \
	src/resolve/expr.c \
	src/resolve/module.c \
	src/resolve/opens.c \
	src/resolve/pattern.c \
	src/resolve/regex_source.c \
	src/resolve/resolve.c \
	src/resolve/scope.c \
	src/resolve/std.c \
	src/resolve/std_source.c \
	src/resolve/surface.c \
	src/resolve/sys_source.c \
	src/resolve/time_source.c \
	src/resolve/type.c \
	src/resolve/warn.c

CHECK_SRC := \
	src/check/bind.c \
	src/check/call.c \
	src/check/callchain.c \
	src/check/apply.c \
	src/check/check.c \
	src/check/composite.c \
	src/check/ctor.c \
	src/check/ctl.c \
	src/check/decl.c \
	src/check/elab.c \
	src/check/entry.c \
	src/check/exhaust.c \
	src/check/exhaust_branch.c \
	src/check/exhaust_leaf.c \
	src/check/exhaust_prod.c \
	src/check/extern.c \
	src/check/expr.c \
	src/check/halt.c \
	src/check/instdepth.c \
	src/check/invert.c \
	src/check/literal.c \
	src/check/member.c \
	src/check/membersvc.c \
	src/check/namespace.c \
	src/check/nsrows.c \
	src/check/op.c \
	src/check/patbind.c \
	src/check/pattern.c \
	src/check/reconcile.c \
	src/check/reqs.c \
	src/check/scheme.c \
	src/check/store.c \
	src/check/subst.c \
	src/check/typename.c \
	src/check/unify.c \
	src/check/worlds.c

LOWER_SRC := \
	src/lower/binop.c \
	src/lower/body.c \
	src/lower/borrow.c \
	src/lower/bundle.c \
	src/lower/call.c \
	src/lower/callargs.c \
	src/lower/callinv.c \
	src/lower/callmember.c \
	src/lower/capfam.c \
	src/lower/capwrap.c \
	src/lower/capture.c \
	src/lower/case.c \
	src/lower/composite.c \
	src/lower/construct.c \
	src/lower/ctl.c \
	src/lower/ctorfam.c \
	src/lower/destructure.c \
	src/lower/derived.c \
	src/lower/derivedcall.c \
	src/lower/derivedeq.c \
	src/lower/derivedfrom.c \
	src/lower/derivedhash.c \
	src/lower/derivedjson.c \
	src/lower/derivedrec.c \
	src/lower/derivedshow.c \
	src/lower/derivedvec.c \
	src/lower/derivedwith.c \
	src/lower/dsvcreg.c \
	src/lower/extcback.c \
	src/lower/extern.c \
	src/lower/extctype.c \
	src/lower/extshape.c \
	src/lower/expr.c \
	src/lower/function.c \
	src/lower/intrinsic.c \
	src/lower/lambda.c \
	src/lower/literal.c \
	src/lower/lower.c \
	src/lower/invert.c \
	src/lower/name.c \
	src/lower/pattern.c \
	src/lower/patlist.c \
	src/lower/subst.c \
	src/lower/tce.c \
	src/lower/type.c \
	src/lower/typeemit.c \
	src/lower/typewalk.c \
	src/lower/unit.c \
	src/lower/unwrap.c \
	src/lower/value.c \
	src/lower/wrap.c

EMIT_SRC := \
	src/emit/emit.c

CLI_SRC := \
	src/cli/main.c \
	src/cli/build.c \
	src/cli/check.c \
	src/cli/front.c \
	src/cli/test.c

LIB_SUPPORT := $(BUILD)/libek_support.a
SUPPORT_OBJ := $(SUPPORT_SRC:src/%.c=$(BUILD)/%.o)

LIB_LEX := $(BUILD)/libek_lex.a
LEX_OBJ := $(LEX_SRC:src/%.c=$(BUILD)/%.o)

LIB_LAYOUT := $(BUILD)/libek_layout.a
LAYOUT_OBJ := $(LAYOUT_SRC:src/%.c=$(BUILD)/%.o)

LIB_PARSE := $(BUILD)/libek_parse.a
PARSE_OBJ := $(PARSE_SRC:src/%.c=$(BUILD)/%.o)

LIB_RESOLVE := $(BUILD)/libek_resolve.a
RESOLVE_OBJ := $(RESOLVE_SRC:src/%.c=$(BUILD)/%.o)

LIB_CHECK := $(BUILD)/libek_check.a
CHECK_OBJ := $(CHECK_SRC:src/%.c=$(BUILD)/%.o)

LIB_LOWER := $(BUILD)/libek_lower.a
LOWER_OBJ := $(LOWER_SRC:src/%.c=$(BUILD)/%.o)

LIB_EMIT := $(BUILD)/libek_emit.a
EMIT_OBJ := $(EMIT_SRC:src/%.c=$(BUILD)/%.o)

CLI_OBJ := $(CLI_SRC:src/%.c=$(BUILD)/%.o)

# link order: later stages first, dependencies last
STAGE_LIBS := $(LIB_EMIT) $(LIB_LOWER) $(LIB_CHECK) $(LIB_RESOLVE) \
	$(LIB_PARSE) $(LIB_LAYOUT) $(LIB_LEX) $(LIB_SUPPORT)

$(LIB_SUPPORT): $(SUPPORT_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_LEX): $(LEX_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_LAYOUT): $(LAYOUT_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_PARSE): $(PARSE_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_RESOLVE): $(RESOLVE_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_CHECK): $(CHECK_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_LOWER): $(LOWER_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_EMIT): $(EMIT_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD)/ekoc: $(CLI_OBJ) $(STAGE_LIBS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc $(CLI_OBJ) $(STAGE_LIBS) -lm -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -MMD -MP -c $< -o $@

# keep object files between runs; they are cheap to reuse, not intermediates
.SECONDARY: $(SUPPORT_OBJ) $(LEX_OBJ) $(LAYOUT_OBJ) $(PARSE_OBJ) \
	$(RESOLVE_OBJ) $(CHECK_OBJ) $(LOWER_OBJ) $(EMIT_OBJ) $(CLI_OBJ)

DEPS += $(SUPPORT_OBJ:.o=.d) $(LEX_OBJ:.o=.d) $(LAYOUT_OBJ:.o=.d) \
	$(PARSE_OBJ:.o=.d) $(RESOLVE_OBJ:.o=.d) $(CHECK_OBJ:.o=.d) \
	$(LOWER_OBJ:.o=.d) $(EMIT_OBJ:.o=.d) $(CLI_OBJ:.o=.d)
