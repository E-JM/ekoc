CC     := cc
AR     := ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2

BUILD  := build

# Stage objects are wired in as they land (ARCHITECTURE.md contract):
#   support -> lex -> layout -> parse -> resolve -> check -> lower -> emit -> cli
SUPPORT_SRC := \
	src/support/arena.c \
	src/support/buf.c \
	src/support/diag.c \
	src/support/utf8.c \
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
	src/resolve/chain.c \
	src/resolve/expr.c \
	src/resolve/module.c \
	src/resolve/pattern.c \
	src/resolve/resolve.c \
	src/resolve/scope.c \
	src/resolve/std.c \
	src/resolve/surface.c \
	src/resolve/type.c

CHECK_SRC := \
	src/check/bind.c \
	src/check/call.c \
	src/check/check.c \
	src/check/composite.c \
	src/check/ctl.c \
	src/check/decl.c \
	src/check/elab.c \
	src/check/entry.c \
	src/check/exhaust.c \
	src/check/exhaust_prod.c \
	src/check/extern.c \
	src/check/expr.c \
	src/check/literal.c \
	src/check/member.c \
	src/check/op.c \
	src/check/patbind.c \
	src/check/pattern.c \
	src/check/reconcile.c \
	src/check/scheme.c \
	src/check/store.c \
	src/check/subst.c \
	src/check/unify.c \
	src/check/worlds.c

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

# link order: later stages first, dependencies last
STAGE_LIBS := $(LIB_CHECK) $(LIB_RESOLVE) $(LIB_PARSE) $(LIB_LAYOUT) \
	$(LIB_LEX) $(LIB_SUPPORT)

UNIT_SRC := $(wildcard tests/unit/*/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/tests/%)

.PHONY: all clean test

# keep object files between runs; they are cheap to reuse, not intermediates
.SECONDARY: $(SUPPORT_OBJ) $(LEX_OBJ) $(LAYOUT_OBJ) $(PARSE_OBJ) \
	$(RESOLVE_OBJ) $(CHECK_OBJ) $(UNIT_BIN:%=%.o)

all: $(LIB_SUPPORT) $(LIB_LEX) $(LIB_LAYOUT) $(LIB_PARSE) $(LIB_RESOLVE) \
	$(LIB_CHECK)

test: all $(UNIT_BIN)
	@status=0; \
	for bin in $(UNIT_BIN); do \
		./$$bin || status=1; \
	done; \
	if [ $$status -eq 0 ]; then echo "ekoc: all unit tests passed"; fi; \
	exit $$status

clean:
	rm -rf build

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

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/tests/%: $(BUILD)/tests/%.o $(STAGE_LIBS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(STAGE_LIBS) -o $@

$(BUILD)/tests/%.o: tests/unit/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests -MMD -MP -c $< -o $@

-include $(SUPPORT_OBJ:.o=.d) $(LEX_OBJ:.o=.d) $(LAYOUT_OBJ:.o=.d) \
	$(PARSE_OBJ:.o=.d) $(RESOLVE_OBJ:.o=.d) $(CHECK_OBJ:.o=.d) \
	$(UNIT_BIN:%=%.d)
