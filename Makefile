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

LIB_SUPPORT := $(BUILD)/libek_support.a
SUPPORT_OBJ := $(SUPPORT_SRC:src/%.c=$(BUILD)/%.o)

UNIT_SRC := $(wildcard tests/unit/*/*.c)
UNIT_BIN := $(UNIT_SRC:tests/unit/%.c=$(BUILD)/tests/%)

.PHONY: all clean test

# keep object files between runs; they are cheap to reuse, not intermediates
.SECONDARY: $(SUPPORT_OBJ) $(UNIT_BIN:%=%.o)

all: $(LIB_SUPPORT)

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

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/tests/%: $(BUILD)/tests/%.o $(LIB_SUPPORT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(LIB_SUPPORT) -o $@

$(BUILD)/tests/%.o: tests/unit/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests -MMD -MP -c $< -o $@

-include $(SUPPORT_OBJ:.o=.d) $(UNIT_BIN:%=%.d)
