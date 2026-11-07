CC     := cc
AR     := ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror

BUILD  := build
JOBS   ?= $(shell nproc 2>/dev/null || echo 1)

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

-include $(DEPS)
