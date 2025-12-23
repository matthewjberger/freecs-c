CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
DEBUG_FLAGS = -g -fsanitize=address -fsanitize=undefined

SRC = freecs.c
HDR = freecs.h
TEST_SRC = freecs_tests.c
TOWER_SRC = examples/tower_defense.c
BOIDS_SRC = examples/boids.c

RAYLIB_FLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm

all: tests

tests: $(SRC) $(HDR) $(TEST_SRC)
	$(CC) $(CFLAGS) -o tests $(SRC) $(TEST_SRC) -lm

tests_debug: $(SRC) $(HDR) $(TEST_SRC)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o tests_debug $(SRC) $(TEST_SRC) -lm

tower_defense: $(SRC) $(HDR) $(TOWER_SRC)
	$(CC) $(CFLAGS) -o tower_defense $(SRC) $(TOWER_SRC) -lm $(RAYLIB_FLAGS)

boids: $(SRC) $(HDR) $(BOIDS_SRC)
	$(CC) $(CFLAGS) -o boids $(SRC) $(BOIDS_SRC) -lm $(RAYLIB_FLAGS)

run_tests: tests
	./tests

clean:
	rm -f tests tests_debug tower_defense boids *.o *.exe

.PHONY: all clean run_tests tests_debug tower_defense boids
