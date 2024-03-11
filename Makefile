# SPDX-License-Identifier: MIT
#
# AVR m328p (Nano) USB Serial to SPI Flipdot Driver
#

PROJECT = avr-flipdrv

# Build objects
OBJECTS = src/main.o
OBJECTS += src/font.o
OBJECTS += src/display.o

# Target binary
TARGET = $(PROJECT).elf

# Listing files
TARGETLIST = $(TARGET:.elf=.lst)

# Compiler
CC = avr-gcc

# Include debugging symbols
DEBUG = -g

# Set language standard
DIALECT = -std=c99 -pedantic

# General code optimisation level
OPTIMISE = -Os
# Mark functions and data so linker can omit dead sections
OPTIMISE += -ffunction-sections -fdata-sections
# Perform link-time optimisation
OPTIMISE += -flto

# Treat all warnings as errors
WARN = -Werror
# Warn on questionable constructs
WARN += -Wall
# Extra warning flags not enabled by -Wall
WARN += -Wextra
# Warn whenever a local variable or type declaration shadows another
WARN += -Wshadow
# Warn if an undefined identifier is evaluated in an #if directive
WARN += -Wundef
# Warn for implicit conversions that may alter a value [annoying]
WARN += -Wconversion

# Avr MCU
AVROPTS = -mmcu=atmega328p -mtiny-stack -ffreestanding

# Clock speed
CPPFLAGS = -DF_CPU=2000000L

# Add include path for headers
CPPFLAGS += -Iinclude

# Extra libraries
LDLIBS =

# Linker flags - omit unused sections
LDFLAGS = -Wl,--gc-sections

# Combined Compiler flags
CFLAGS = $(DIALECT) $(DEBUG) $(OPTIMISE) $(WARN) $(AVROPTS)
LCFLAGS = CFLAGS

# binutils
OBJCOPY = avr-objcopy
SIZE = avr-size
NM = avr-nm
NMFLAGS = -n -r
DISFLAGS = -d -S -m avr5
OBJDUMP = avr-objdump

# programmer
AVRDUDE = avrdude
PARTNO = m328p
PROGRAMMER = avrisp2
DUDECMD = $(AVRDUDE) -c $(PROGRAMMER) -p $(PARTNO)
EFUSE = 0xfd
LFUSE = 0x7f
HFUSE = 0xd6
LOCKBYTE = 0xff


# Default target is $(TARGET)
.PHONY: elf
elf: $(TARGET)

$(OBJECTS): Makefile

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $(TARGET) $(OBJECTS)

# Override compilation recipe for assembly files
%.o: %.s
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Add recipe for listing files
%.lst: %.o
	$(OBJDUMP) $(DISFLAGS) $< > $@
%.lst: %.elf
	$(OBJDUMP) $(DISFLAGS) $< > $@

.PHONY: size
size: $(TARGET)
	$(SIZE) $(TARGET)

.PHONY: nm
nm: $(TARGET)
	$(NM) $(NMFLAGS) $(TARGET)

.PHONY: list
list: $(TARGETLIST)

.PHONY: erase
erase:
	$(DUDECMD) -e

.PHONY: upload
upload: $(TARGET)
	$(DUDECMD) -U flash:w:$(TARGET):e

.PHONY: fuse
fuse: Makefile
	$(DUDECMD) -U efuse:w:$(EFUSE):m -U hfuse:w:$(HFUSE):m -U lfuse:w:$(LFUSE):m -U lock:w:$(LOCKBYTE):m

.PHONY: clean
clean:
	-rm -f $(TARGET) $(OBJECTS) $(TARGETLIST)

.PHONY: requires
requires:
	sudo apt-get install gcc-avr binutils-avr avr-libc avrdude

.PHONY: help
help:
	@echo
	@echo Targets:
	@echo " elf [default]   build all objects, link and write $(TARGET)"
	@echo " size            list $(TARGET) section sizes"
	@echo " nm              list all defined symbols in $(TARGET)"
	@echo " list            create text listing for $(TARGET)"
	@echo " erase           bulk erase flash on target"
	@echo " fuse            re-write fuses"
	@echo " upload          write $(TARGET) to flash and verify"
	@echo " clean           remove all intermediate files and logs"
	@echo " requires	install development dependencies"
	@echo
