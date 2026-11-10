# Runtime library (ekort) linked into compiled Eko programs; it is not a
# pipeline stage and stands outside the stage link order. Included by the
# root Makefile, in order after src/makefile — not runnable standalone.
ifeq ($(strip $(BUILD)),)
$(error run make at the repository root; rt/makefile is an include fragment)
endif

RT_SRC := rt/value.c rt/rc.c rt/num.c rt/numbits.c rt/nummath.c rt/panic.c \
	rt/frames.c rt/string.c rt/bytes.c rt/list.c rt/key.c rt/dict.c rt/hamt.c \
	rt/closure.c \
	rt/fmt.c rt/parse.c rt/convert.c rt/strops.c rt/charops.c \
	rt/chartype.c rt/xid_tables.c rt/bytesops.c \
	rt/cap.c rt/bundle.c rt/env.c rt/args.c rt/genword.c rt/genval.c rt/genio.c \
	rt/conc.c rt/worldcall.c \
	rt/gentime.c rt/genmono.c rt/genduration.c rt/geninfo.c rt/in.c rt/clock.c rt/rand.c \
	rt/file.c rt/fs.c rt/fsdir.c rt/path.c rt/range.c rt/ptr.c \
	rt/set.c rt/vector.c rt/jsontext.c rt/json.c rt/timeparse.c

LIB_RT := $(BUILD)/libekort.a
RT_OBJ := $(RT_SRC:rt/%.c=$(BUILD)/rt/%.o)

$(LIB_RT): $(RT_OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD)/rt/%.o: rt/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Irt -MMD -MP -c $< -o $@

.SECONDARY: $(RT_OBJ)

DEPS += $(RT_OBJ:.o=.d)
