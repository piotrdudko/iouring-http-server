CC = clang
CFLAGS = -g -DDEBUG -Wall -Wextra
LDFLAGS = -luring
CLIB_DIR = ../clib
BIN_DIR = bin
CLIB_SOURCES = $(wildcard $(CLIB_DIR)/*.c)
CLIB_HEADERS = $(wildcard $(CLIB_DIR)/*.h)

.PHONY: clean all run

all: $(BIN_DIR)/program

$(BIN_DIR)/program: main.c $(CLIB_DIR)/libiouring_helpers.a | $(BIN_DIR)
	$(CC) $(CFLAGS) main.c -I$(CLIB_DIR) -L$(CLIB_DIR) -liouring_helpers $(LDFLAGS) -o $@

$(CLIB_DIR)/libiouring_helpers.a: $(CLIB_SOURCES) $(CLIB_HEADERS)
	$(MAKE) -C $(CLIB_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run: $(BIN_DIR)/program
	./$(BIN_DIR)/program

clean:
	rm -rf $(BIN_DIR)
	$(MAKE) -C $(CLIB_DIR) clean
