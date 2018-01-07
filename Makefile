
MCU := msp430g2553
SOURCENAMES := main.c
HEADERNAMES :=

CC := msp430-elf-gcc
CCFLAGS := -Wall -Wextra -g -Os -mmcu=$(MCU) -I/opt/ti/mspgcc/msp430-elf/include -DNDEBUG

SRCDIR := src
OUTFILE := main.elf

SOURCES := $(addprefix $(SRCDIR)/,$(SOURCENAMES))
HEADERS := $(addprefix $(SRCDIR)/,$(HEADERNAMES))

.PHONY: all
all: $(OUTFILE)

.PHONY: clean
clean:
	rm -f $(OUTFILE)

.PHONY: run
run: $(OUTFILE)
	sudo mspdebug rf2500 "prog $(OUTFILE)"

$(OUTFILE): $(SOURCES) $(HEADERS)
	$(CC) $(CCFLAGS) -o $@ $(SOURCES)
