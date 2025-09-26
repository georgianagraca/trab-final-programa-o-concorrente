# Compilador e flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -I./src/libtslog
LDFLAGS = -lpthread

# Diretórios
SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj

# Fontes e Objetos
LOG_SRC = $(SRC_DIR)/libtslog/tslog.c
LOG_OBJ = $(OBJ_DIR)/tslog.o

TEST_SRC = $(TEST_DIR)/test_logging.c
TEST_OBJ = $(OBJ_DIR)/test_logging.o

# Alvo principal
TARGET = test_logging

all: $(TARGET)

# Regra para o executável de teste
$(TARGET): $(TEST_OBJ) $(LOG_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Regras para compilar os arquivos objeto
$(OBJ_DIR)/%.o: $(SRC_DIR)/libtslog/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para limpar os arquivos gerados
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean