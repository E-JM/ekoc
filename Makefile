CC     := cc
AR     := ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror

BUILD  := build
JOBS   ?= $(shell nproc 2>/dev/null || echo 1)

# Fragment order is load-bearing: the test rules below read STAGE_LIBS from
# src/makefile and LIB_RT from rt/makefile; every fragment appends DEPS, and
# the -include of the dependency files must stay the last line of this file.
.DELETE_ON_ERROR:

include src/makefile rt/makefile

UNIT_SRC := $(wildcard tests/unit/*/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/tests/%)

.PHONY: all clean test conformance

# keep object files between runs; they are cheap to reuse, not intermediates
.SECONDARY: $(UNIT_BIN:%=%.o)

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
-include $(DEPS)
