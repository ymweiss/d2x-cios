# devkitARM path
DEVKITARM ?=	/opt/devkitARM

# Prefix
PREFIX	=	$(DEVKITARM)/bin/arm-eabi-

# Executables
CC	=	$(PREFIX)gcc
LD	=	$(PREFIX)gcc
STRIP	=	../stripios

# Strip address
SADDR	=	0x13700000

# Committer date for reproducible builds
SOURCE_DATE_EPOCH ?= $(shell git log -1 --pretty=%ct)
ifdef SOURCE_DATE_EPOCH
	BUILD_DATE ?= $(shell LC_ALL=C date -u -d "@$(SOURCE_DATE_EPOCH)" "+'%b %e %Y'" 2>/dev/null || LC_ALL=C date -u -r "$(SOURCE_DATE_EPOCH)" "+'%b %e %Y'" 2>/dev/null || LC_ALL=C date -u "+'%b %e %Y'")
	BUILD_TIME ?= $(shell LC_ALL=C date -u -d "@$(SOURCE_DATE_EPOCH)" "+'%H:%M:%S'" 2>/dev/null || LC_ALL=C date -u -r "$(SOURCE_DATE_EPOCH)" "+'%H:%M:%S'" 2>/dev/null || LC_ALL=C date -u "+'%H:%M:%S'")
else
	BUILD_DATE ?= $(shell date "+'%b %e %Y'")
	BUILD_TIME ?= $(shell date "+'%H:%M:%S'")
endif

# Flags
ARCH	=	-mcpu=arm926ej-s -mthumb -mthumb-interwork -mbig-endian
CFLAGS	=	$(ARCH) -I. -I../cios-lib -ffreestanding -fno-builtin -ffunction-sections -fomit-frame-pointer -Os -Wall -Wno-builtin-macro-redefined -D__TIME__=\"$(BUILD_TIME)\" -D__DATE__=\"$(BUILD_DATE)\"
LDFLAGS	=	$(ARCH) -nostartfiles -nostdlib -Wl,-T,link.ld,-Map,$(TARGET).map -Wl,--gc-sections -Wl,-static

# Libraries
LIBS	=	../cios-lib/cios-lib.a

# Target
TARGET	=	mload-module

# Objects
OBJS	=	debug.o		\
		detect.o	\
		elf.o		\
		gecko.o		\
		gpio.o		\
		main.o		\
		patches.o	\
		start.o		\
		swi.o		\
		swi_asm.o	\
		tid.o


$(TARGET).elf: $(OBJS) $(LIBS)
	@echo -e " LD\t$@"
	@$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -lgcc -o $@.orig
	@$(STRIP) $@.orig $@
	
# strip $(SADDR)

%.o: %.s
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -D_LANGUAGE_ASSEMBLY -c -x assembler-with-cpp -o $@ $<

%.o: %.c
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo -e "Cleaning..."
	@rm -f $(OBJS) $(TARGET).elf $(TARGET).elf.orig $(TARGET).map
