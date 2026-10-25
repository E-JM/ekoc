CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2
AR      ?= ar

# Stage objects are wired in as they land (src/ARCHITECTURE contract):
#   support -> lex -> layout -> parse -> resolve -> check -> lower -> emit -> cli
# The build must remain: one C99 compiler, no other toolchain dependency.

.PHONY: all clean test

all:
	@echo "ekoc: stage sources not yet present; see ARCHITECTURE.md"

clean:
	rm -rf build

test:
	@echo "ekoc: conformance runner lands with the first pipeline stages"
