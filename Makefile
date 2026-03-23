#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/gba_rules

#---------------------------------------------------------------------------------
# project settings
#---------------------------------------------------------------------------------
TARGET := $(notdir $(CURDIR))
TARGET_SRAM := $(TARGET)_sram
TARGET_FLASH := $(TARGET)_end_of_flash

BUILD_SRAM := build_sram
BUILD_FLASH := build_end_of_flash
ASSET_BUILD := build

SRC_C := $(wildcard src/*.c)
SRC_CPP := $(wildcard src/*.cpp)
SRC_S := $(wildcard src/*.s)

OBJ_C_SRAM := $(patsubst src/%.c,$(BUILD_SRAM)/%.o,$(SRC_C))
OBJ_CPP_SRAM := $(patsubst src/%.cpp,$(BUILD_SRAM)/%.o,$(SRC_CPP))
OBJ_S_SRAM := $(patsubst src/%.s,$(BUILD_SRAM)/%.o,$(SRC_S))
OFILES_SRAM := $(OBJ_C_SRAM) $(OBJ_CPP_SRAM) $(OBJ_S_SRAM)

OBJ_C_FLASH := $(patsubst src/%.c,$(BUILD_FLASH)/%.o,$(SRC_C))
OBJ_CPP_FLASH := $(patsubst src/%.cpp,$(BUILD_FLASH)/%.o,$(SRC_CPP))
OBJ_S_FLASH := $(patsubst src/%.s,$(BUILD_FLASH)/%.o,$(SRC_S))
OFILES_FLASH := $(OBJ_C_FLASH) $(OBJ_CPP_FLASH) $(OBJ_S_FLASH)

DEPS := $(OBJ_C_SRAM:.o=.d) $(OBJ_CPP_SRAM:.o=.d) $(OBJ_S_SRAM:.o=.d) \
	$(OBJ_C_FLASH:.o=.d) $(OBJ_CPP_FLASH:.o=.d) $(OBJ_S_FLASH:.o=.d)

ifeq ($(strip $(SRC_CPP)),)
LD := $(CC)
else
LD := $(CXX)
endif

LIBS := -lmm -lgba
LIBDIRS := $(LIBGBA) $(DEVKITPRO)/libtonc
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ARCH := -mthumb -mthumb-interwork

CFLAGS_COMMON := -g -Wall -O2 \
	-mcpu=arm7tdmi -mtune=arm7tdmi \
	$(ARCH)

INCLUDE := -iquote $(CURDIR)/include \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(ASSET_BUILD)

CFLAGS_SRAM := $(CFLAGS_COMMON) $(INCLUDE) -DDEFAULT_SAVE_BACKEND=SAVE_BACKEND_SRAM
CFLAGS_FLASH := $(CFLAGS_COMMON) $(INCLUDE) -DDEFAULT_SAVE_BACKEND=SAVE_BACKEND_FLASH_END_8K

CXXFLAGS_SRAM := $(CFLAGS_SRAM) -fno-rtti -fno-exceptions
CXXFLAGS_FLASH := $(CFLAGS_FLASH) -fno-rtti -fno-exceptions

ASFLAGS := -g $(ARCH)

.PHONY: all clean format run run_sram run_end_of_flash

#---------------------------------------------------------------------------------
all: $(TARGET_SRAM).gba $(TARGET_FLASH).gba

#---------------------------------------------------------------------------------
run: run_sram

run_sram: $(TARGET_SRAM).gba
	@mGBA $(TARGET_SRAM).gba

run_end_of_flash: $(TARGET_FLASH).gba
	@mGBA $(TARGET_FLASH).gba

#---------------------------------------------------------------------------------
$(ASSET_BUILD) $(BUILD_SRAM) $(BUILD_FLASH):
	@[ -d $@ ] || mkdir -p $@

#---------------------------------------------------------------------------------
$(TARGET_SRAM).elf: OFILES := $(OFILES_SRAM)
$(TARGET_SRAM).elf: LDFLAGS := -g $(ARCH) -Wl,-Map,$(BUILD_SRAM)/$(TARGET_SRAM).map
$(TARGET_SRAM).elf: $(OFILES_SRAM)

$(TARGET_FLASH).elf: OFILES := $(OFILES_FLASH)
$(TARGET_FLASH).elf: LDFLAGS := -g $(ARCH) -Wl,-Map,$(BUILD_FLASH)/$(TARGET_FLASH).map
$(TARGET_FLASH).elf: $(OFILES_FLASH)

$(OFILES_SRAM): $(ASSET_BUILD)/image.h
$(OFILES_FLASH): $(ASSET_BUILD)/image.h

#---------------------------------------------------------------------------------
$(BUILD_SRAM)/%.o: src/%.c | $(BUILD_SRAM) $(ASSET_BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD_SRAM)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS_SRAM) -c $< -o $@ $(ERROR_FILTER)

$(BUILD_SRAM)/%.o: src/%.cpp | $(BUILD_SRAM) $(ASSET_BUILD)
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(BUILD_SRAM)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS_SRAM) -c $< -o $@ $(ERROR_FILTER)

$(BUILD_SRAM)/%.o: src/%.s | $(BUILD_SRAM) $(ASSET_BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD_SRAM)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

$(BUILD_FLASH)/%.o: src/%.c | $(BUILD_FLASH) $(ASSET_BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD_FLASH)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS_FLASH) -c $< -o $@ $(ERROR_FILTER)

$(BUILD_FLASH)/%.o: src/%.cpp | $(BUILD_FLASH) $(ASSET_BUILD)
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(BUILD_FLASH)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS_FLASH) -c $< -o $@ $(ERROR_FILTER)

$(BUILD_FLASH)/%.o: src/%.s | $(BUILD_FLASH) $(ASSET_BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD_FLASH)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
tools/bmp: tools/bmp.c
	gcc -Wall -Wextra -O2 -o $@ $<

$(ASSET_BUILD)/image.bmp: data/image.png | $(ASSET_BUILD)
	magick $< -flatten BMP3:$@

$(ASSET_BUILD)/image.h: tools/bmp $(ASSET_BUILD)/image.bmp | $(ASSET_BUILD)
	@tools/bmp $(ASSET_BUILD)/image.bmp $@

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(ASSET_BUILD) $(BUILD_SRAM) $(BUILD_FLASH) \
		$(TARGET_SRAM).elf $(TARGET_SRAM).gba $(TARGET_SRAM).sav \
		$(TARGET_FLASH).elf $(TARGET_FLASH).gba $(TARGET_FLASH).sav \
		tools/bmp

#---------------------------------------------------------------------------------
format:
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/#if \(.*\)/if (\1) { \/\/ #if-move/g'
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/#endif/} \/\/ #endif-move/g'
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs clang-format -i
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/if (\(.*\)) { \/\/ #if-move/#if \1/g'
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/} \/\/ #endif-move/#endif/g'

-include $(DEPS)
