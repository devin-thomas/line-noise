.SUFFIXES:

TARGET     := line-noise
GAME_TITLE := LINE NOISE!
GAME_CODE  := LNSE
MAKER_CODE := 00

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
LIBGBA    := $(DEVKITPRO)/libgba

CC      := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY := $(DEVKITARM)/bin/arm-none-eabi-objcopy
SIZE    := $(DEVKITARM)/bin/arm-none-eabi-size
GBAFIX  := $(DEVKITPRO)/tools/bin/gbafix

ASSET_DIR := ../line-noise-gba-asset-pack/assets/line-noise/generated/grit
MAP_DIR   := ../line-noise-gba-asset-pack/assets/line-noise/generated/maps

AUDIO_WAVS := \
	assets/audio/ring_subscriber.wav \
	assets/audio/plug_click.wav \
	assets/audio/cord_move.wav \
	assets/audio/crosstalk.wav \
	assets/audio/packet_transfer.wav \
	assets/audio/ground_clamp.wav \
	assets/audio/verify.wav \
	assets/audio/dawn_chime.wav

ASSET_NAMES := \
	animated_console_tiles \
	call_bursts \
	caller_portraits \
	city_monitor_tiles \
	cord_clamps \
	cord_palette_banks \
	cord_tiles \
	crosstalk_fx \
	cursor \
	exchange_tiles \
	font \
	hud_icons \
	hud_panels \
	monitor_markers \
	plugs \
	scene_palettes \
	service_badges \
	signal_packets \
	status_lamps \
	title_screen_tiles \
	uppercut_labs_logo \
	wordmark

C_SOURCES   := $(wildcard source/*.c)
ASM_SOURCES := $(wildcard source/*.s)
C_OBJECTS   := $(patsubst source/%.c,build/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst source/%.s,build/%.o,$(ASM_SOURCES))
ASSET_OBJECTS := $(addprefix build/assets/,$(addsuffix .o,$(ASSET_NAMES)))
MAP_OBJECTS := build/assets/title_screen_map.o
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS) $(ASSET_OBJECTS) $(MAP_OBJECTS)
DEPS := $(OBJECTS:.o=.d)

ARCH := -mcpu=arm7tdmi -mtune=arm7tdmi -mthumb -mthumb-interwork
WARNINGS := -Wall -Wextra -Wshadow -Wconversion -Wstrict-prototypes
INCLUDES := -Iinclude -I$(ASSET_DIR) -I$(MAP_DIR) \
	-isystem $(LIBGBA)/include
CFLAGS := -g -O2 $(ARCH) $(WARNINGS) -ffunction-sections -fdata-sections \
	-fno-strict-aliasing $(INCLUDES) -MMD -MP
ASFLAGS := -g $(ARCH) -x assembler-with-cpp
LDFLAGS := -g $(ARCH) -specs=gba.specs -Wl,-Map,$(TARGET).map \
	-Wl,--gc-sections -L$(LIBGBA)/lib
LIBS := -lgba

.PHONY: all clean verify size

all: $(TARGET).gba

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary "$<" "$@"
	$(GBAFIX) "$@" "-t$(GAME_TITLE)" -c$(GAME_CODE) -m$(MAKER_CODE)

$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o "$@"

build/%.o: source/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

build/audio_data.o: source/audio_data.s $(AUDIO_WAVS)
	@mkdir -p "$(dir $@)"
	$(CC) $(ASFLAGS) -c "$<" -o "$@"

build/%.o: source/%.s
	@mkdir -p "$(dir $@)"
	$(CC) $(ASFLAGS) -c "$<" -o "$@"

build/assets/%.o: $(ASSET_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

build/assets/title_screen_map.o: $(MAP_DIR)/title_screen_map.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

verify: $(TARGET).gba
	@python3 ../tools/verify_rom.py "$(TARGET).gba" "$(TARGET).elf"

size: $(TARGET).elf
	$(SIZE) "$<"

clean:
	@rm -rf build "$(TARGET).elf" "$(TARGET).gba" "$(TARGET).map"

-include $(DEPS)
