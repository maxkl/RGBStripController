
MCU := msp430g2553
SOURCENAMES := main.c
HEADERNAMES :=

CC := msp430-elf-gcc
CCFLAGS := -Wall -Wextra -O2 -mmcu=$(MCU)

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
