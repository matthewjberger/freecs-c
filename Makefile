CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
DEBUG_FLAGS = -g -fsanitize=address -fsanitize=undefined

HDR = freecs.h
TEST_SRC = freecs_tests.c
TOWER_SRC = examples/tower_defense.c
BOIDS_SRC = examples/boids.c

RAYLIB_FLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm

all: tests

tests: $(HDR) $(TEST_SRC)
	$(CC) $(CFLAGS) -o tests $(TEST_SRC) -lm

tests_debug: $(HDR) $(TEST_SRC)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o tests_debug $(TEST_SRC) -lm

tower_defense: $(HDR) $(TOWER_SRC)
	$(CC) $(CFLAGS) -o tower_defense $(TOWER_SRC) -lm $(RAYLIB_FLAGS)

boids: $(HDR) $(BOIDS_SRC)
	$(CC) $(CFLAGS) -o boids $(BOIDS_SRC) -lm $(RAYLIB_FLAGS)

run_tests: tests
	./tests

clean:
	rm -f tests tests_debug tower_defense boids *.o *.exe

.PHONY: all clean run_tests tests_debug tower_defense boids
