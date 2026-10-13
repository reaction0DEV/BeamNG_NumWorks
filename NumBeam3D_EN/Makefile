# NumBeam3D pour la calculatrice NumWorks.
#   make          compile output/numbeam3d.nwa
#   make check    liens comme sur la calculatrice + taille installee
#   make run      installe sur la calculatrice branchee en USB
#   make clean    supprime output/
#   make EXTRA=-DMAX_PHYS=1   option: moins de RAM (1 seul bot physique) si l'edition de liens deborde
# Dossier attendu: src/main.c  src/sim.h  src/epsilon_app.h  src/epsilon_files.h  icon.png  Makefile
Q ?= @
CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
BUILD_DIR = output
NAME = numbeam3d
SRC = src/main.c
HEADERS = src/sim.h src/epsilon_app.h src/epsilon_files.h

CFLAGS = -std=c11 $(shell $(NWLINK) eadk-cflags-device)
CFLAGS += -Os -Wall -Wextra -Wno-unused-parameter -fno-math-errno -fsingle-precision-constant
CFLAGS += $(EXTRA)
CFLAGS += -fno-tree-loop-distribute-patterns -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

.PHONY: build run check clean
build: $(BUILD_DIR)/$(NAME).nwa

run: $(BUILD_DIR)/$(NAME).nwa
	$(Q) $(NWLINK) install-nwa $<

check: $(BUILD_DIR)/$(NAME).nwa
	$(Q) $(NWLINK) nwa-bin $< $(BUILD_DIR)/$(NAME).bin
	@echo "BIN     $(BUILD_DIR)/$(NAME).bin: $$(wc -c < $(BUILD_DIR)/$(NAME).bin) bytes"

$(BUILD_DIR)/main.o: $(SRC) $(HEADERS) | $(BUILD_DIR)
	@echo "CC      $<"
	$(Q) $(CC) $(CFLAGS) -c $(SRC) -o $@

$(BUILD_DIR)/$(NAME).nwa: $(BUILD_DIR)/main.o $(BUILD_DIR)/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -lm -o $@
	$(Q) arm-none-eabi-strip --strip-unneeded $@
	$(Q) arm-none-eabi-size $@

$(BUILD_DIR)/icon.o: icon.png | $(BUILD_DIR)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

clean:
	$(Q) rm -rf $(BUILD_DIR)
