CC = clang
CFLAGS = -g -DDEBUG -Wall -Wextra -luring
SRC_DIR = .
BIN_DIR = bin

.PHONY: clean all

all: $(BIN_DIR)/program

$(BIN_DIR)/program: $(SRC_DIR)/main.c $(SRC_DIR)/logging.c $(SRC_DIR)/userdata.h $(SRC_DIR)/logging.h $(SRC_DIR)/bufring.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c $(SRC_DIR)/logging.c -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run: $(BIN_DIR)/program
	./$(BIN_DIR)/program

clean:
	rm -rf $(BIN_DIR)
