CC      ?= cc
STD     ?= -std=c99
WARN    ?= -Wall -Wextra -Wpedantic
OPT     ?= -O2
CFLAGS  ?= $(STD) $(OPT) $(WARN)
BUILD   ?= build
EXE     ?=

BIN := $(BUILD)/c99gui$(EXE)
SRC := src/c99gui.c

.PHONY: all strict static test check clean install
all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

# ISO C only: no platform headers, line-based input.
strict: CFLAGS += -DSTRICT_C99
strict: BIN := $(BUILD)/c99gui-strict$(EXE)
strict:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

# Fully static binary (glibc/musl Linux).
static: LDFLAGS += -static
static: BIN := $(BUILD)/c99gui-static$(EXE)
static:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

test: $(BIN)
	sh tests/smoke.sh $(BIN)

# Same warnings the CI uses: everything on, warnings are errors.
check: WARN := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wstrict-prototypes -Werror
check: clean test

clean:
	rm -rf $(BUILD)

PREFIX ?= /usr/local
install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/c99gui
