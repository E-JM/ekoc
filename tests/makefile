# Unit suites, the test gate, and the conformance gate. Included by the
# root Makefile, in order after src/makefile and rt/makefile: the rules
# below read STAGE_LIBS (src) and LIB_RT (rt), and every suite runs with
# the repository root as its working directory. Not runnable standalone.
ifeq ($(strip $(BUILD)),)
$(error run make at the repository root; tests/makefile is an include fragment)
endif

UNIT_SRC := $(wildcard tests/unit/*/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/tests/%)

# One run target per suite lets make -j schedule suites beside their
# compilation. The run targets are never created as files, so every make
# run executes every suite; they must not be .PHONY, because phony
# targets skip the implicit rule search that pairs them with the binaries.
UNIT_RUN := $(UNIT_BIN:$(BUILD)/tests/%=$(BUILD)/run/%)

.PHONY: test conformance

# keep object files and binaries between runs; they are cheap to reuse,
# not intermediates — the run targets make the binaries chain intermediates
.SECONDARY: $(UNIT_BIN) $(UNIT_BIN:%=%.o)

test: $(BUILD)/ekoc $(UNIT_RUN)
	@echo "ekoc: all unit tests passed"

$(BUILD)/run/%: $(BUILD)/tests/%
	@mkdir -p $(dir $@)
	@./$< >$@.log 2>&1 && { rm -f $@.log $@.fail; echo "ok $*"; } \
		|| { cat $@.log; mv $@.log $@.fail; exit 1; }

# these two suites shell out to ./build/ekoc at runtime, so they wait
# for the compiler binary even under make -j
$(BUILD)/run/cli/test $(BUILD)/run/cli/run: $(BUILD)/ekoc

# The conformance gate: every module of the suite through the real
# pipeline. A module with test bindings runs as its own root; a library
# module rides the check pipeline. An E-lower rejection fails the gate
# outright: a program check accepted must lower.
conformance: $(BUILD)/ekoc $(LIB_RT)
	@set -- tests/conformance/*.eko; [ -f "$$1" ] || \
		{ echo "ekoc: no conformance files"; exit 1; }
	@printf '%s\n' tests/conformance/*.eko | \
		xargs -P $(JOBS) -n 1 tests/conformance/gate.sh $(BUILD)/ekoc \
		&& echo "ekoc: conformance suite passed"

$(BUILD)/tests/%: $(BUILD)/tests/%.o $(STAGE_LIBS) $(LIB_RT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(STAGE_LIBS) $(LIB_RT) -lm -pthread -o $@

# the collection suite exercises the CLI's shared front, which is a
# program object, not a stage library
$(BUILD)/tests/cli/test_collect: $(BUILD)/tests/cli/test_collect.o \
		$(BUILD)/cli/front.o $(STAGE_LIBS) $(LIB_RT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(BUILD)/cli/front.o \
		$(STAGE_LIBS) $(LIB_RT) -lm -pthread -o $@

$(BUILD)/tests/%.o: tests/unit/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Irt -Itests -MMD -MP -c $< -o $@

DEPS += $(UNIT_BIN:%=%.d)
