Q ?= @
CC = arm-none-eabi-gcc
NWLINK = npx --yes -- nwlink@1.0.0
BUILD_DIR = output
CFLAGS = -std=gnu11 $(shell $(NWLINK) eadk-cflags-device) -Os -Wall -fno-math-errno -fsingle-precision-constant
CFLAGS += -fdata-sections -ffunction-sections -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level -Wl,--gc-sections -flinker-output=nolto-rel
build: $(BUILD_DIR)/numbeam3d.nwa
$(BUILD_DIR)/main.o: src/main.c src/sim.h | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -c src/main.c -o $@
$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	$(Q) $(NWLINK) png-icon-o $< $@
$(BUILD_DIR)/numbeam3d.nwa: $(BUILD_DIR)/main.o $(BUILD_DIR)/icon.o
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
check: $(BUILD_DIR)/numbeam3d.nwa
	$(Q) $(NWLINK) nwa-bin $< $(BUILD_DIR)/numbeam3d.bin && ls -l $(BUILD_DIR)
$(BUILD_DIR):
	$(Q) mkdir -p $@
.PHONY: build check
