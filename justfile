# freecs-c build commands

set windows-shell := ["sh", "-cu"]

CFLAGS := "-Wall -Wextra -std=c11 -O2"

# Default recipe
default:
    @just --list

# Install raylib via scoop (recommended for Windows)
install-raylib:
    scoop install raylib-mingw

# Install build tools via scoop
install-tools:
    scoop install mingw

# Full setup: install all dependencies
setup: install-tools install-raylib

# Build tests
build-tests:
    export PATH="$HOME/scoop/apps/mingw/current/bin:$PATH" && gcc {{CFLAGS}} -o tests.exe freecs.c freecs_tests.c

# Build tests with debug flags
build-tests-debug:
    export PATH="$HOME/scoop/apps/mingw/current/bin:$PATH" && gcc {{CFLAGS}} -g -o tests_debug.exe freecs.c freecs_tests.c

# Run tests
test: build-tests
    ./tests.exe

# Build tower defense game
build-tower:
    export PATH="$HOME/scoop/apps/mingw/current/bin:$PATH" && gcc {{CFLAGS}} -o tower_defense.exe freecs.c examples/tower_defense.c \
        -I"$HOME/scoop/apps/raylib-mingw/current/raylib-5.5_win64_mingw-w64/include" \
        -L"$HOME/scoop/apps/raylib-mingw/current/raylib-5.5_win64_mingw-w64/lib" \
        -lraylib -lopengl32 -lgdi32 -lwinmm

# Build and run tower defense
tower: build-tower
    ./tower_defense.exe

# Clean build artifacts
clean:
    rm -f tests.exe tests_debug.exe tower_defense.exe *.o
