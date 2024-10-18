.DELETE_ON_ERROR:
.SUFFIXES:

# Makefile                                                                      
# Written by Leon Krieg <info@madcow.dev>

# ==============================================================================
# GENERAL SETTINGS
# ==============================================================================

VERBOSE   := false
ARCH      := m1284
MCU       := atmega1284p
FREQ      := 8000000UL
ISP       := usbasp
CC        := avr-gcc
LD        := $(CC)
OBJCOPY   := avr-objcopy
AVD       := avrdude
SIM       := simavr
MKDIR     := mkdir -p
RMR       := rm -rf
GIT       := git

LFUSE     := 0xC2
HFUSE     := 0xD7
EFUSE     := 0xFF
LOCK      := 0xFF

SRCDIR    := src
BINDIR    := bin
TMPDIR    := $(BINDIR)/build
TARGET    := $(BINDIR)/core.hex
ELFFILE   := $(BINDIR)/core.elf
LOGFILE   := $(BINDIR)/core.log

CPPFLAGS  := -DF_CPU=$(FREQ) -I$(SRCDIR)
CFLAGS    := -mmcu=$(MCU) -Os -std=c99 -Wall -Wextra -Werror
OCFLAGS   := -j .text -j .data -O ihex
LDFLAGS   := -mmcu=$(MCU) -Wl,-u,vfprintf -lprintf_flt
LDLIBS    := -lm

# ==============================================================================
# TARGET FILE LISTS (DERIVED FROM SOURCE TREE)
# ==============================================================================

PATHS     := $(shell find "$(SRCDIR)" -type d -printf '%P ' 2>/dev/null)
FILES     := $(shell find "$(SRCDIR)" -type f -name "*.c" -printf '%P ' 2>/dev/null)
TMPDIRS   := $(BINDIR) $(TMPDIR) $(PATHS:%=$(TMPDIR)/%)
OBJECTS   := $(FILES:%.c=$(TMPDIR)/%.o)
DEPENDS   := $(FILES:%.c=$(TMPDIR)/%.d)

# ==============================================================================
# AUXILIARY TARGETS
# ==============================================================================

.PHONY: all
all: flash

.PHONY: flash
flash: $(TARGET)
	$(E) "[AVD] Flashing..."
	$(Q) $(AVD)                   \
	       -l$(LOGFILE) -B375kHz  \
	       -c $(ISP) -p $(ARCH)   \
	       -U lfuse:w:$(LFUSE):m  \
	       -U hfuse:w:$(HFUSE):m  \
	       -U efuse:w:$(EFUSE):m  \
	       -U lock:w:$(LOCK):m    \
	       -U flash:w:$<

.PHONY: simulate
simulate: $(TARGET)
	$(E) "[SIM] $<"
	$(Q) $(SIM) -m $(MCU) -f $(FREQ) $<

.PHONY: clean
clean:
	$(E) "[REM] $(TARGET)"
	$(Q) $(RMR) $(TARGET)
	$(E) "[REM] $(ELFFILE)"
	$(Q) $(RMR) $(ELFFILE)
	$(E) "[REM] $(LOGFILE)"
	$(Q) $(RMR) $(LOGFILE)
	$(E) "[REM] $(TMPDIR)"
	$(Q) $(RMR) $(TMPDIR)

.PHONY: distclean
distclean: clean
	$(E) "[REM] $(BINDIR)"
	$(Q) $(RMR) $(BINDIR)

$(TMPDIRS):
	$(E) "[DIR] $@"
	$(Q) $(MKDIR) $@

# ==============================================================================
# PRIMARY BUILD TARGET AND PATTERN RULES
# ==============================================================================

# We must expand the prerequisite lists a second time to resolve path variable
# $(@D). This means folders can be set as explicit dependencies and created in
# the $TMPDIRS rule. This is better than relying on Make to honor the order of
# prerequisites for the primary target and we will not have to call mkdir for
# each build step preemptively.

.SECONDEXPANSION:

$(TARGET): $(ELFFILE) | $$(@D)
	$(E) "[HEX] $@"
	$(Q) $(OBJCOPY) $(OCFLAGS) $< $@

$(ELFFILE): $(OBJECTS) $(DEPENDS) | $$(@D)
	$(E) "[ELF] $@"
	$(Q) $(LD) -o $@ $(LDFLAGS) $(OBJECTS) $(LDLIBS)

$(TMPDIR)/%.o: $(SRCDIR)/%.c | $$(@D)
	$(E) "[OBJ] $@"
	$(Q) $(CC) -c -o $@ $(CFLAGS) $(CPPFLAGS) $<

$(TMPDIR)/%.d: $(SRCDIR)/%.c | $$(@D)
	$(E) "[DEP] $@"
	$(Q) $(CC) -c -o $@ $(CFLAGS) $(CPPFLAGS) -MM -MT $(@:.d=.o) $<

# ==============================================================================
# MAKE PREPROCESSOR INCLUDES AND CONDITIONALS
# ==============================================================================

# Load header dependency rules
include $(wildcard $(DEPENDS))

# Sanity check
ifeq ($(strip $(OBJECTS)),)
$(error No sources found in '$(SRCDIR)/')
endif

# Handle verbosity setting
ifneq ($(VERBOSE), false)
E = @true
else
E = @echo
Q = @
endif
