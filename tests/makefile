# Unit suites, the test gate, and the conformance gate. Included by the
# root Makefile, in order after src/makefile and rt/makefile: the rules
# below read STAGE_LIBS (src) and LIB_RT (rt), and every suite runs with
# the repository root as its working directory. Not runnable standalone.
ifeq ($(strip $(BUILD)),)
$(error run make at the repository root; tests/makefile is an include fragment)
endif

UNIT_SRC := $(wildcard tests/unit/*/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/tests/%)

.PHONY: test conformance

# keep object files between runs; they are cheap to reuse, not intermediates
.SECONDARY: $(UNIT_BIN:%=%.o)

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
conformance: $(BUILD)/ekoc $(LIB_RT)
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
