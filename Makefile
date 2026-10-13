CC ?= cc
PKG_CONFIG ?= pkg-config
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags sdl2 glesv2)
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl2 glesv2) -lm
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run clean check-deps test test-tcp
all: build/fastfps

check-deps:
	@$(PKG_CONFIG) --exists sdl2 glesv2 || { echo "Bibliotheques manquantes : SDL2 et OpenGL ES."; echo "Installe : sudo apt install build-essential pkg-config libsdl2-dev libgles-dev"; exit 1; }

build/fastfps: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

build/%.o: src/%.c | check-deps
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

run: all
	./build/fastfps

clean:
	rm -rf build

TEST_SOURCES := tests/network.c src/network.c src/discovery.c src/protocol.c src/game.c src/progression.c src/physics.c src/world.c src/math.c src/mesh.c
build/test-network: $(TEST_SOURCES) src/fastfps.h src/network.h src/config.h | check-deps
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -I src $(TEST_SOURCES) $(LDLIBS) -o $@

test: build/test-network
	./build/test-network

test-tcp: build/test-network
	./build/test-network --tcp

-include $(DEPS)
