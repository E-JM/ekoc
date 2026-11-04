CC     := cc
AR     := ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror

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
	src/resolve/cap_source.c \
	src/resolve/chain.c \
	src/resolve/embedded.c \
	src/resolve/expr.c \
	src/resolve/module.c \
	src/resolve/opens.c \
	src/resolve/pattern.c \
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
	src/check/literal.c \
	src/check/member.c \
	src/check/membersvc.c \
	src/check/namespace.c \
	src/check/nsrows.c \
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

LOWER_SRC := \
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
	src/lower/ctl.c \
	src/lower/destructure.c \
	src/lower/derived.c \
	src/lower/derivedcall.c \
	src/lower/derivedeq.c \
	src/lower/derivedfrom.c \
	src/lower/derivedhash.c \
	src/lower/derivedjson.c \
	src/lower/derivedrec.c \
	src/lower/derivedshow.c \
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

LIB_LOWER := $(BUILD)/libek_lower.a
LOWER_OBJ := $(LOWER_SRC:src/%.c=$(BUILD)/%.o)

EMIT_SRC := \
	src/emit/emit.c

LIB_EMIT := $(BUILD)/libek_emit.a
EMIT_OBJ := $(EMIT_SRC:src/%.c=$(BUILD)/%.o)

CLI_SRC := \
	src/cli/main.c \
	src/cli/build.c \
	src/cli/check.c \
	src/cli/front.c \
	src/cli/test.c

CLI_OBJ := $(CLI_SRC:src/%.c=$(BUILD)/%.o)

# rt is the runtime library (ekort) linked into compiled Eko programs;
# it is not a pipeline stage and stands outside the stage link order.
RT_SRC := rt/value.c rt/rc.c rt/num.c rt/numbits.c rt/nummath.c rt/panic.c \
	rt/string.c rt/bytes.c rt/list.c rt/dict.c rt/closure.c \
	rt/fmt.c rt/parse.c rt/convert.c rt/strops.c rt/charops.c rt/bytesops.c \
	rt/cap.c rt/capval.c rt/capenv.c rt/in.c rt/clock.c rt/rand.c \
	rt/file.c rt/fs.c rt/fsdir.c rt/path.c rt/range.c rt/ptr.c \
	rt/set.c rt/jsontext.c rt/json.c
LIB_RT := $(BUILD)/libekort.a
RT_OBJ := $(RT_SRC:rt/%.c=$(BUILD)/rt/%.o)

# link order: later stages first, dependencies last
STAGE_LIBS := $(LIB_EMIT) $(LIB_LOWER) $(LIB_CHECK) $(LIB_RESOLVE) \
	$(LIB_PARSE) $(LIB_LAYOUT) $(LIB_LEX) $(LIB_SUPPORT)

UNIT_SRC := $(wildcard tests/unit/*/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/tests/%)

.PHONY: all clean test conformance

# keep object files between runs; they are cheap to reuse, not intermediates
.SECONDARY: $(SUPPORT_OBJ) $(LEX_OBJ) $(LAYOUT_OBJ) $(PARSE_OBJ) \
	$(RESOLVE_OBJ) $(CHECK_OBJ) $(LOWER_OBJ) $(EMIT_OBJ) $(CLI_OBJ) \
	$(RT_OBJ) $(UNIT_BIN:%=%.o)

all: $(LIB_SUPPORT) $(LIB_LEX) $(LIB_LAYOUT) $(LIB_PARSE) $(LIB_RESOLVE) \
	$(LIB_CHECK) $(LIB_LOWER) $(LIB_EMIT) $(LIB_RT) $(BUILD)/ekoc

test: all $(UNIT_BIN)
	@status=0; \
	for bin in $(UNIT_BIN); do \
		./$$bin || status=1; \
	done; \
	if [ $$status -eq 0 ]; then echo "ekoc: all unit tests passed"; fi; \
	exit $$status

# The conformance gate: every module of the suite through the real
# pipeline. A module with test bindings runs as its own root; a library
# module rides the check pipeline. An E-lower rejection fails the gate
# outright: a program check accepted must lower.
conformance: $(BUILD)/ekoc
	@status=0; \
	for f in tests/conformance/*.eko; do \
		out=`./build/ekoc test $$f 2>&1`; rc=$$?; \
		if echo "$$out" | grep -q 'E-lower'; then \
			echo "gate $$f: a checked program was rejected in lower"; \
			echo "$$out" | grep 'E-lower' | head -3; \
			status=1; \
		elif echo "$$out" | grep -q 'declares no test bindings'; then \
			out=`./build/ekoc check $$f 2>&1`; \
			if echo "$$out" | grep -q 'E-lower'; then \
				echo "gate $$f: a checked program was rejected in lower"; \
				echo "$$out" | grep 'E-lower' | head -3; \
				status=1; \
			elif echo "$$out" | grep ' E-' | grep -qv 'E-entry-point'; then \
				echo "fail $$f"; \
				echo "$$out" | grep ' E-' | grep -v 'E-entry-point' | head -3; \
				status=1; \
			fi; \
		elif [ $$rc -ne 0 ]; then \
			echo "fail $$f"; echo "$$out" | tail -5; \
			status=1; \
		fi; \
	done; \
	if [ $$status -eq 0 ]; then echo "ekoc: conformance suite passed"; fi; \
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

$(LIB_LOWER): $(LOWER_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_EMIT): $(EMIT_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIB_RT): $(RT_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD)/ekoc: $(CLI_OBJ) $(STAGE_LIBS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc $(CLI_OBJ) $(STAGE_LIBS) -lm -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/rt/%.o: rt/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Irt -MMD -MP -c $< -o $@

$(BUILD)/tests/%: $(BUILD)/tests/%.o $(STAGE_LIBS) $(LIB_RT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(STAGE_LIBS) $(LIB_RT) -lm -o $@

# the collection suite exercises the CLI's shared front, which is a
# program object, not a stage library
$(BUILD)/tests/cli/test_collect: $(BUILD)/tests/cli/test_collect.o \
		$(BUILD)/cli/front.o $(STAGE_LIBS) $(LIB_RT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(BUILD)/cli/front.o \
		$(STAGE_LIBS) $(LIB_RT) -lm -o $@

$(BUILD)/tests/%.o: tests/unit/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Irt -Itests -MMD -MP -c $< -o $@

-include $(SUPPORT_OBJ:.o=.d) $(LEX_OBJ:.o=.d) $(LAYOUT_OBJ:.o=.d) \
	$(PARSE_OBJ:.o=.d) $(RESOLVE_OBJ:.o=.d) $(CHECK_OBJ:.o=.d) \
	$(LOWER_OBJ:.o=.d) $(EMIT_OBJ:.o=.d) $(CLI_OBJ:.o=.d) \
	$(RT_OBJ:.o=.d) $(UNIT_BIN:%=%.d)
