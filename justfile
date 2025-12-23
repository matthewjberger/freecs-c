# freecs-c build commands

set windows-shell := ["sh", "-cu"]

CFLAGS := "-Wall -Wextra -std=c11 -O2"
RAYLIB_INC := "$HOME/scoop/apps/raylib-mingw/current/raylib-5.5_win64_mingw-w64/include"
RAYLIB_LIB := "$HOME/scoop/apps/raylib-mingw/current/raylib-5.5_win64_mingw-w64/lib"
RAYLIB_FLAGS := "-lraylib -lopengl32 -lgdi32 -lwinmm"

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

# Run tests
test: build-tests
    ./tests.exe

# Build tower defense game
build-tower:
    export PATH="$HOME/scoop/apps/mingw/current/bin:$PATH" && gcc {{CFLAGS}} -o tower_defense.exe freecs.c examples/tower_defense.c \
        -I{{RAYLIB_INC}} -L{{RAYLIB_LIB}} {{RAYLIB_FLAGS}}

# Build and run tower defense
tower: build-tower
    ./tower_defense.exe

# Build boids simulation
build-boids:
    export PATH="$HOME/scoop/apps/mingw/current/bin:$PATH" && gcc {{CFLAGS}} -o boids.exe freecs.c examples/boids.c \
        -I{{RAYLIB_INC}} -L{{RAYLIB_LIB}} {{RAYLIB_FLAGS}}

# Build and run boids
boids: build-boids
    ./boids.exe

# Clean build artifacts
clean:
    rm -f *.exe *.o *.pdb *.ilk
