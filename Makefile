CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g 

SRC_DIR = ./src
OBJ_DIR = ./obj
BIN_DIR = ./bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/pragma

.PHONY: all clean local

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

local: $(EXECUTABLE)
	@mkdir -p ~/bin/
	cp $(EXECUTABLE) ~/bin/
	@echo "Installed pragma to ~/bin/pragma. (ensure that ~/bin is in PATH)"

clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLE)

