CC      ?= cc
STD     ?= -std=c99
WARN    ?= -Wall -Wextra -Wpedantic
OPT     ?= -O2
CFLAGS  ?= $(STD) $(OPT) $(WARN)
BUILD   ?= build

# OS Detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    LDFLAGS_GUI += -lgdi32 -luser32
    EXE := .exe
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    LDFLAGS_GUI += -lgdi32 -luser32
    EXE := .exe
else ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
    LDFLAGS_GUI += -lgdi32 -luser32
    EXE := .exe
else ifeq ($(UNAME_S),Darwin)
    LDFLAGS_GUI += -framework Cocoa -framework CoreGraphics
else
    # Linux / BSD / Unix: check if X11 is available
    HAS_X11 := $(shell printf '\#include <X11/Xlib.h>\nint main(){return 0;}' | $(CC) -x c - -lX11 -o /dev/null 2>/dev/null && echo yes || echo no)
    ifeq ($(HAS_X11),yes)
        LDFLAGS_GUI += -lX11
    else
        CFLAGS += -DC99GUI_HEADLESS
    endif
endif

# Program names
TUI_BIN    := $(BUILD)/c99gui-tui$(EXE)
PIXEL_BIN  := $(BUILD)/c99gui-pixel$(EXE)
VECTOR_BIN := $(BUILD)/c99gui-vector$(EXE)
SUITE_BIN  := $(BUILD)/c99gui-suite$(EXE)

# Source files
TUI_SRC    := src/tui/c99gui.c
PIXEL_SRC  := src/pixel/gui.c
VECTOR_SRC := src/vector/gui.c
SUITE_SRC  := src/suite/gui.c

.PHONY: all tui pixel vector suite headless test check clean install

all: tui pixel vector suite

tui: $(TUI_BIN)
pixel: $(PIXEL_BIN)
vector: $(VECTOR_BIN)
suite: $(SUITE_BIN)

$(TUI_BIN): $(TUI_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TUI_SRC) -o $@

$(PIXEL_BIN): $(PIXEL_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(PIXEL_SRC) -o $@ $(LDFLAGS_GUI)

$(VECTOR_BIN): $(VECTOR_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(VECTOR_SRC) -o $@ $(LDFLAGS_GUI)

$(SUITE_BIN): $(SUITE_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SUITE_SRC) -o $@ $(LDFLAGS_GUI)

headless: CFLAGS += -DC99GUI_HEADLESS
headless: tui pixel vector suite

test: all
	chmod +x tests/smoke.sh tests/tui_smoke.sh
	sh tests/tui_smoke.sh $(TUI_BIN)
	sh tests/smoke.sh $(PIXEL_BIN)
	sh tests/smoke.sh $(VECTOR_BIN)
	sh tests/smoke.sh $(SUITE_BIN)

check: WARN := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
check: clean test

clean:
	rm -rf $(BUILD) snapshot*.bmp

PREFIX ?= /usr/local
install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TUI_BIN) $(DESTDIR)$(PREFIX)/bin/c99gui-tui
	install -m 755 $(PIXEL_BIN) $(DESTDIR)$(PREFIX)/bin/c99gui-pixel
	install -m 755 $(VECTOR_BIN) $(DESTDIR)$(PREFIX)/bin/c99gui-vector
	install -m 755 $(SUITE_BIN) $(DESTDIR)$(PREFIX)/bin/c99gui-suite
