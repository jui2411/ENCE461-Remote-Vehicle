# Need to define:
# OPT optimisation level, e.g. -O2
# MCU name of microcontroller
# RUN_MODE either ROM or RAM
# MAT91LIB_DIR path to mat91lib
# PERIPHERALS list of peripherals to build

# Note.  If weird errors occur it is because the makefile fragments are
# loaded in the wrong order.  mmuclib.mk should be included last.

ifndef MCU
$(error MCU undefined, this needs to be defined in the Makefile)
endif

ifndef RUN_MODE
RUN_MODE = ROM	
endif

ifndef OPT
OPT = -O0
endif

ifndef TOOLCHAIN
TOOLCHAIN = arm-none-eabi
endif

ifneq (, $(shell which $(TOOLCHAIN)-gdb))
GDB = $(TOOLCHAIN)-gdb
else
# This supersedes arm-none-eabi-gdb
GDB = gdb-multiarch
endif

TARGET_MAP = $(addsuffix .map, $(basename $(TARGET)))

ifneq (, $(findstring SAM7, $(MCU)))
FAMILY = sam7
else
ifneq (, $(findstring SAM4S, $(MCU)))
FAMILY = sam4s
else
$(error unknown family)
endif
endif


all: $(TARGET)

include $(MAT91LIB_DIR)/$(FAMILY)/$(FAMILY).mk

SCRIPTS = $(MAT91LIB_DIR)/$(FAMILY)/scripts
LDSCRIPTS = $(MAT91LIB_DIR)/$(FAMILY)

# To keep things simple, don't link with g++.  This is only
# needed if use c++ libraries.
#LD = $(TOOLCHAIN)-g++
LD = $(TOOLCHAIN)-gcc


CC = $(TOOLCHAIN)-gcc
CXX = $(TOOLCHAIN)-g++
OBJCOPY = $(TOOLCHAIN)-objcopy
SIZE = $(TOOLCHAIN)-size
DEL = rm -f

# Disable run time type information so virtual members work
# in the interim.
CXXFLAGS += -fno-rtti

INCLUDES += -I.


ifeq ($(RUN_MODE), RAM)
LDFLAGS +=-L$(LDSCRIPTS) -T$(MCU)-RAM.ld
else 
LDFLAGS +=-L$(LDSCRIPTS) -T$(MCU)-ROM.ld
endif

# Hack.  FIXME
SRC += syscalls.c

CSRC = $(filter %.c, $(SRC))
#CCSRC = $(filter %.cc %.cpp, $(SRC))
CCSRC = $(filter %.cpp, $(SRC))

ifneq ($(CCSRC), )
LDFLAGS += -lstdc++
endif


OBJDIR = objs
DEPDIR = deps

# Dirty hack: add _x suffix for C++ files as a workaround for Windows'
# lack of case discrimination where Adc.o is considered the same as
# adc.o.  This could be conditionally applied depending on the OS.  A
# better fix to create a hierarchical build tree or to prefix the
# filename with the library name.  An even better approach is to
# rewrite using cmake.

OBJ1 = $(CCSRC:.cpp=_x.o) $(CSRC:.c=.o) 

DEP1 = $(CCSRC:.cpp=_x.d) $(CSRC:.c=.d) 

# Create list of object and dependency files.  Note, sort removes duplicates.
OBJS = $(sort $(addprefix $(OBJDIR)/, $(notdir $(sort $(OBJ1)))))
DEPS = $(sort $(addprefix $(DEPDIR)/, $(notdir $(sort $(DEP1)))))
SRC_DIRS = $(sort $(dir $(SRC)))


VPATH += $(SRC_DIRS)

include $(MAT91LIB_DIR)/peripherals.mk

EXTRA_OBJS = $(OBJDIR)/crt0.o $(OBJDIR)/mcu.o

print-drivers:
	@echo $(DRIVERS)

print-deps:
	@echo $(DEPS)

print-objs:
	@echo $(OBJS)

print-src-dirs:
	@echo $(SRC_DIRS)
	@echo $(VPATH)

print-cflags:
	@echo $(CFLAGS)

print-src:
	@echo $(SRC)

print-csrc:
	@echo $(CSRC)

print-ccsrc:
	@echo $(CCSRC)

print-vpath:
	@echo $(VPATH)

print-includes:
	@echo $(INCLUDES)

# Create dirs if they do not exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(DEPDIR):
	mkdir -p $(DEPDIR)

$(OBJS): | $(OBJDIR)

$(DEPS): | $(DEPDIR)

ifeq (1, 0)
# Strategy 1 for automatic dependency generation.  Before the .c files
# are compiled, dependency files (.d) are created (one for each .c
# file) using the -MM compiler option.  sed is used to add the .d file
# as a target (in addition to the .o file) so that .d file is
# re-created if any of the pre-requisites are modified.  While sed is
# a standard Unix utility, it is not always available on Windows.
# This is slightly faster than strategy 2 since the .d files are only
# created when a pre-requisite is modified.

# Rule to compile .cpp file to .o file.
$(OBJDIR)/%_x.o: %.cpp Makefile
	$(CXX) -c $(CXXFLAGS) $< -o $@

# Rule to compile .c file to .o file.
$(OBJDIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

# Rule to create .d file from .cpp file.
$(DEPDIR)/%_x.d: %.cpp
	@set -e; $(DEL) $@; \
	$(CC) -MM $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)x_\.o[ :]*,$(OBJDIR)/x_\1.o $@ : ,g' < $@.$$$$ > $@; \
	$(DEL) $@.$$$$

# Rule to create .d file from .c file.
$(DEPDIR)/%.d: %.c
	@set -e; $(DEL) $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(OBJDIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	$(DEL) $@.$$$$
else
# Strategy 2 for automatic dependency generation.  Dependency files
# are included if they exist.  When a .c file is compiled, a
# dependency file (.d) is created.  This is slightly slower than
# strategy 1 since the .d file is regenerated whenever a file is
# compiled.

$(OBJDIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@
# Generate dependencies to see if object file needs recompiling.
	@printf "$(OBJDIR)/" > deps/$*.d
	$(CC) -MM $(CFLAGS) $< >> deps/$*.d

$(OBJDIR)/%_x.o: %.cpp Makefile
	$(CXX) -c $(CXXFLAGS) $< -o $@
# Generate dependencies to see if object file needs recompiling.
	@printf "$(OBJDIR)/" > deps/$*_x.d
	$(CXX) -MM $(CXXFLAGS) $< >> deps/$*_x.d
endif

# Link object files to form output file.
$(TARGET): $(DEPDIR) $(OBJS) $(EXTRA_OBJS)
	$(LD) $(OBJS) $(EXTRA_OBJS) $(LDFLAGS) -o $@ -lm -Wl,-Map=$(TARGET_MAP),--cref
	$(SIZE) $@

# Include the dependency files.
-include $(DEPS)

# Remove non-source files.
.PHONY: clean
clean: 
	-$(DEL) *.o *.out *.hex *.bin *.elf *.d *.lst *.map *.sym *.lss *.cfg *.ocd *~
	-$(DEL) -r $(OBJDIR) $(DEPDIR)

# Program the device.
.PHONY: program
program: $(TARGET)
	$(GDB) -batch -x $(SCRIPTS)/program.gdb $^

# Reset the device.
.PHONY: reset
reset:
	$(GDB) -batch -x $(SCRIPTS)/reset.gdb

# Enable booting from flash.
.PHONY: bootflash
bootflash:
	$(GDB) -batch -x $(SCRIPTS)/bootflash.gdb

# Attach debugger.
.PHONY: debug
debug:
	$(GDB)  -x $(SCRIPTS)/debug.gdb $(TARGET)

