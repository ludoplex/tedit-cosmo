# tedit-cosmo Makefile
# Build with: make (GUI) or make cli (CLI-only)

CC ?= cosmocc
CXX ?= cosmoc++
CFLAGS = -O2 -Wall -Wextra -std=c11 -Iinclude
CXXFLAGS = -O2 -Wall -Wextra -std=c++11 -Iinclude

# Core sources (platform-independent)
SRC_CORE = \
	src/main.c \
	src/app.c \
	src/buffer.c \
	src/editor.c \
	src/file.c \
	src/config.c \
	src/menu.c \
	src/build.c \
	src/syntax.c \
	src/util.c \
	src/script.c \
	src/history.c \
	src/backup.c

# CLI backend
SRC_CLI = src/platform/cli.c

# cimgui GUI backend
SRC_GUI = src/platform/cimgui_backend.c

# cimgui sources (vendored)
SRC_CIMGUI = \
	deps/cimgui/cimgui.cpp \
	deps/cimgui/imgui/imgui.cpp \
	deps/cimgui/imgui/imgui_draw.cpp \
	deps/cimgui/imgui/imgui_tables.cpp \
	deps/cimgui/imgui/imgui_widgets.cpp \
	deps/cimgui/imgui/imgui_demo.cpp

TARGET = tedit.com

.PHONY: all cli gui clean setup-deps

# Default: CLI (most portable)
all: cli

# CLI-only build
cli: $(SRC_CORE) $(SRC_CLI)
	@echo "Building tedit-cosmo (CLI)"
	$(CC) $(CFLAGS) -DPLATFORM_CLI -o $(TARGET) $(SRC_CORE) $(SRC_CLI)
	@echo "Built: $(TARGET)"

# GUI build (requires cimgui vendored)
gui: check-deps $(SRC_CORE) $(SRC_GUI) $(SRC_CIMGUI)
	@echo "Building tedit-cosmo (GUI with cimgui)"
	$(CXX) $(CXXFLAGS) -DPLATFORM_GUI -Ideps/cimgui -Ideps/cimgui/imgui \
		-o $(TARGET) $(SRC_CORE) $(SRC_GUI) $(SRC_CIMGUI) -lglfw -lGL -ldl -lpthread
	@echo "Built: $(TARGET)"

check-deps:
	@test -d deps/cimgui || (echo "Run: make setup-deps" && exit 1)

setup-deps:
	@echo "Cloning cimgui..."
	git clone --recursive https://github.com/ludoplex/cimgui.git deps/cimgui

clean:
	rm -f $(TARGET) *.o

