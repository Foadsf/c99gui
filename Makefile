CC      ?= cc
STD     ?= -std=c99
WARN    ?= -Wall -Wextra -Wpedantic
OPT     ?= -O2
CFLAGS  ?= $(STD) $(OPT) $(WARN)
BUILD   ?= build
EXE     ?=

# OS Detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    LDFLAGS += -lgdi32 -luser32
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    LDFLAGS += -lgdi32 -luser32
else ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
    LDFLAGS += -lgdi32 -luser32
else ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -framework Cocoa -framework CoreGraphics
else
    # Linux / BSD / Unix: check if X11 is available
    HAS_X11 := $(shell printf '\#include <X11/Xlib.h>\nint main(){return 0;}' | $(CC) -x c - -lX11 -o /dev/null 2>/dev/null && echo yes || echo no)
    ifeq ($(HAS_X11),yes)
        LDFLAGS += -lX11
    else
        CFLAGS += -DC99GUI_HEADLESS
    endif
endif

BIN := $(BUILD)/gui$(EXE)
SRC := src/gui.c

.PHONY: all headless test check clean install
all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

headless: CFLAGS += -DC99GUI_HEADLESS
headless: BIN := $(BUILD)/gui-headless$(EXE)
headless:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

test: $(BIN)
	sh tests/smoke.sh $(BIN)

check: WARN := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
check: clean test

clean:
	rm -rf $(BUILD) snapshot.bmp

PREFIX ?= /usr/local
install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/gui
