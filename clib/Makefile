CC = clang
AR = ar
CFLAGS = -g -DDEBUG -Wall -Wextra -fPIC
LDFLAGS = -luring

LIB_DIR = .
OBJ_DIR = obj
TARGET = libiouring_helpers.a

SOURCES = logging.c bufring.c userdata.c
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(LIB_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
