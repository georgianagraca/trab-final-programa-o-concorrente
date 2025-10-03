# Compilador e flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -I./src
LDFLAGS = -lpthread

# Diretórios
SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj

# --- Fontes e Objetos ---
LOG_SRC = $(SRC_DIR)/libtslog/tslog.c
LOG_OBJ = $(OBJ_DIR)/tslog.o

SERVER_SRC = $(SRC_DIR)/server/server.c
SERVER_OBJ = $(OBJ_DIR)/server.o

CLIENT_SRC = $(SRC_DIR)/client/client.c
CLIENT_OBJ = $(OBJ_DIR)/client.o

# --- Alvos Executáveis ---
SERVER_TARGET = server
CLIENT_TARGET = client
TEST_TARGET = test_logging

all: $(SERVER_TARGET) $(CLIENT_TARGET)

# --- Regras de Build ---
$(SERVER_TARGET): $(SERVER_OBJ) $(LOG_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_TARGET): $(TEST_DIR)/test_logging.c $(LOG_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# --- Regras para compilar os arquivos objeto ---
$(OBJ_DIR)/%.o: $(SRC_DIR)/libtslog/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/server/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/client/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para limpar os arquivos gerados
clean:
	rm -rf $(OBJ_DIR) $(SERVER_TARGET) $(CLIENT_TARGET) $(TEST_TARGET)

.PHONY: all clean