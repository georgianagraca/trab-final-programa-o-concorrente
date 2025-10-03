#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "libtslog/tslog.h"

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

// --- Estruturas de Dados Compartilhadas ---
static int client_sockets[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Funções de Gerenciamento de Clientes (com proteção de mutex) ---

void add_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        client_sockets[client_count++] = socket;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    int i;
    for (i = 0; i < client_count; i++) {
        if (client_sockets[i] == socket) {
            // Move o último cliente para a posição do que saiu para evitar buracos
            client_sockets[i] = client_sockets[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_message(const char* message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        // Envia para todos, menos para quem enviou a mensagem original
        if (client_sockets[i] != sender_socket) {
            if (write(client_sockets[i], message, strlen(message)) < 0) {
                LOG_ERROR("Falha ao enviar mensagem broadcast.");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// --- Lógica da Thread do Cliente ---

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg); // Libera memória alocada para o argumento

    char buffer[BUFFER_SIZE];
    int read_size;

    // Loop principal para receber dados do cliente
    while ((read_size = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[read_size] = '\0';
        
        char log_msg[BUFFER_SIZE + 50];
        sprintf(log_msg, "Mensagem recebida do socket %d: %s", client_socket, buffer);
        LOG_INFO(log_msg);

        broadcast_message(buffer, client_socket);
        memset(buffer, 0, sizeof(buffer));
    }

    // Se read() retorna 0, o cliente desconectou. Se retorna -1, houve um erro.
    if (read_size == 0) {
        char log_msg[50];
        sprintf(log_msg, "Cliente (socket %d) desconectado.", client_socket);
        LOG_INFO(log_msg);
    } else {
        LOG_ERROR("Erro ao ler do socket.");
    }

    // Limpeza
    close(client_socket);
    remove_client(client_socket);
    return NULL;
}

// --- Função Principal do Servidor ---

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    logger_init();
    LOG_INFO("Iniciando o servidor de chat...");

    // 1. Criar Socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        LOG_ERROR("Falha ao criar o socket.");
        return 1;
    }

    // Permite reutilizar o endereço para evitar erros de "Address already in use"
    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 2. Configurar Endereço do Servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer IP
    server_addr.sin_port = htons(port);

    // 3. Bind do Socket à Porta
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Bind falhou.");
        return 1;
    }
    LOG_INFO("Servidor escutando na porta especificada.");

    // 4. Listen (Aguardar por Conexões)
    listen(server_socket, 5);
    LOG_INFO("Aguardando conexões de clientes...");

    // 5. Loop de Aceite de Conexões
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            LOG_ERROR("Accept falhou.");
            continue;
        }

        // Log da nova conexão
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        char log_msg[100];
        sprintf(log_msg, "Nova conexão de %s:%d (socket %d)", client_ip, ntohs(client_addr.sin_port), client_socket);
        LOG_INFO(log_msg);

        add_client(client_socket);

        // Cria uma thread para cuidar do novo cliente
        pthread_t client_thread;
        int* new_socket_ptr = malloc(sizeof(int));
        *new_socket_ptr = client_socket;

        if (pthread_create(&client_thread, NULL, handle_client, (void*)new_socket_ptr) < 0) {
            LOG_ERROR("Falha ao criar a thread do cliente.");
            free(new_socket_ptr);
        }
    }

    close(server_socket);
    logger_destroy();
    return 0;
}