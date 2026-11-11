CC     := cc
AR     := ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror

BUILD  := build
JOBS   ?= $(shell nproc 2>/dev/null || echo 1)

# the fragments' rules would otherwise take the default goal from the
# first target they carry
.DEFAULT_GOAL := all

# Fragment order is load-bearing: tests/makefile reads STAGE_LIBS from
# src/makefile and LIB_RT from rt/makefile; every fragment appends DEPS,
# and the -include of the dependency files must stay the last line.
.DELETE_ON_ERROR:

include src/makefile rt/makefile tests/makefile

.PHONY: all

all: $(LIB_SUPPORT) $(LIB_LEX) $(LIB_LAYOUT) $(LIB_PARSE) $(LIB_RESOLVE) \
	$(LIB_CHECK) $(LIB_LOWER) $(LIB_EMIT) $(LIB_RT) $(BUILD)/ekoc

clean:
	rm -rf build

# The embedded source tables regenerate from their release files; the
# tool is never a build step, and the byte-comparison suite under
# tests/unit/resolve is the drift gate.
REGEN := tools/gen_embedded_source.py

.PHONY: regen
regen:
	python3 $(REGEN) std lib/std.eko src/resolve/std_source.c
	python3 $(REGEN) time lib/std/time.eko src/resolve/time_source.c
	python3 $(REGEN) regex lib/std/regex.eko src/resolve/regex_source.c
	python3 $(REGEN) cap cap/cap.eko src/resolve/cap_source.c
	python3 $(REGEN) sys cap/sys.eko src/resolve/sys_source.c

-include $(DEPS)
