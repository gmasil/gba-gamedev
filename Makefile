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
BUILD := build

SRC_C := $(wildcard src/*.c)
SRC_CPP := $(wildcard src/*.cpp)
SRC_S := $(wildcard src/*.s)

OBJ_C := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC_C))
OBJ_CPP := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRC_CPP))
OBJ_S := $(patsubst src/%.s,$(BUILD)/%.o,$(SRC_S))

OFILES := $(OBJ_C) $(OBJ_CPP) $(OBJ_S)

DEPS := $(OBJ_C:.o=.d) $(OBJ_CPP:.o=.d) $(OBJ_S:.o=.d)

ifeq ($(strip $(SRC_CPP)),)
LD := $(CC)
else
LD := $(CXX)
endif

LIBS := -lmm -lgba
LIBDIRS := $(LIBGBA) $(DEVKITPRO)/libtonc
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ARCH := -mthumb -mthumb-interwork

CFLAGS := -g -Wall -O2 \
	-mcpu=arm7tdmi -mtune=arm7tdmi \
	$(ARCH)

INCLUDE := -iquote $(CURDIR)/include \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)

CFLAGS += $(INCLUDE)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := -g $(ARCH)
LDFLAGS := -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map

.PHONY: all clean format run

#---------------------------------------------------------------------------------
all: $(TARGET).gba

#---------------------------------------------------------------------------------
run: all
	@mGBA $(TARGET).gba

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@

#---------------------------------------------------------------------------------
$(TARGET).gba: $(TARGET).elf

$(TARGET).elf: $(OFILES)

$(OFILES): $(BUILD)/image.h

#---------------------------------------------------------------------------------
$(BUILD)/%.o: src/%.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(BUILD)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

$(BUILD)/%.o: src/%.s | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
tools/bmp: tools/bmp.c
	gcc -Wall -Wextra -O2 -o $@ $<

$(BUILD)/image.bmp: data/image.png | $(BUILD)
	magick $< -flatten BMP3:$@

$(BUILD)/image.h: tools/bmp $(BUILD)/image.bmp | $(BUILD)
	@tools/bmp $(BUILD)/image.bmp $@

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).gba $(TARGET).sav tools/bmp

#---------------------------------------------------------------------------------
format:
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/#if \(.*\)/if (\1) { \/\/ #if-move/g'
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/#endif/} \/\/ #endif-move/g'
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs clang-format -i
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/if (\(.*\)) { \/\/ #if-move/#if \1/g'
	find src -type f \( -name '*.c' -o -name '*.h' \) | xargs sed -i '' 's/} \/\/ #endif-move/#endif/g'

-include $(DEPS)
