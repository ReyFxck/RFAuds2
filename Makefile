PS2DEV ?= /usr/local/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk
PS2SDKSRC ?= $(PS2SDK)

EE_CC ?= $(shell command -v ee-gcc 2>/dev/null || command -v mips64r5900el-ps2-elf-gcc 2>/dev/null)
EE_AR ?= $(shell command -v ee-ar 2>/dev/null || command -v mips64r5900el-ps2-elf-ar 2>/dev/null)

BUILD := build
EE_CFLAGS := -G0 -O2 -Wall -Wextra -D_EE \
    -Iinclude -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include

EE_OBJS := \
    $(BUILD)/client.o \
    $(BUILD)/core.o

.PHONY: all clean irx check package host-test

all: $(BUILD)/librfauds2.a $(BUILD)/rfauds2.irx

$(BUILD):
	mkdir -p $@

$(BUILD)/client.o: src/ee/client.c include/rfauds2/rfauds2.h include/rfauds2/rpc.h | $(BUILD)
	$(EE_CC) $(EE_CFLAGS) -c $< -o $@

$(BUILD)/core.o: src/ee/core.c include/rfauds2/rfauds2.h | $(BUILD)
	$(EE_CC) $(EE_CFLAGS) -c $< -o $@

$(BUILD)/librfauds2.a: $(EE_OBJS)
	$(EE_AR) rcs $@ $^

$(BUILD)/rfauds2.irx: src/iop/src/main.c src/iop/src/spu2_direct.c src/iop/src/spu2_direct.h src/iop/src/imports.lst src/iop/src/irx_imports.h include/rfauds2/rpc.h | $(BUILD)
	$(MAKE) -C src/iop clean all PS2SDKSRC="$(PS2SDKSRC)" PS2SDK="$(PS2SDK)"
	cp src/iop/irx/rfauds2.irx $@

irx: $(BUILD)/rfauds2.irx

host-test: | $(BUILD)
	cc -std=c99 -O2 -Wall -Wextra -Werror \
		-Itests/include -Iinclude \
		src/ee/core.c tests/audio_test.c \
		-o $(BUILD)/rfauds2-host-test
	$(BUILD)/rfauds2-host-test

check: all
	@test -s $(BUILD)/librfauds2.a
	@test -s $(BUILD)/rfauds2.irx
	@echo "RFAuds2 build OK"

package: check
	mkdir -p $(BUILD)/package/include/rfauds2 $(BUILD)/package/lib $(BUILD)/package/iop
	cp include/rfauds2/*.h $(BUILD)/package/include/rfauds2/
	cp $(BUILD)/librfauds2.a $(BUILD)/package/lib/
	cp $(BUILD)/rfauds2.irx $(BUILD)/package/iop/

clean:
	rm -rf $(BUILD)
	$(MAKE) -C src/iop clean PS2SDKSRC="$(PS2SDKSRC)" PS2SDK="$(PS2SDK)" || true
