#define _POSIX_C_SOURCE 200809L
#define MAX_MODERATOR_WORDS 500
#include <signal.h>
#include <ctype.h>
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
#define HISTORY_SIZE 15 // Armazena as últimas 15 mensagens

volatile sig_atomic_t g_server_running = 1;
static int g_server_socket = -1;


void filter_message(char* message);
void send_private_message(const char* message, const char* sender_nickname, const char* target_nickname, int sender_socket);


typedef struct {
    int socket;
    char nickname[BUFFER_SIZE];
} Client;

static Client clients[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// OBS: A lista de palavras é apenas um exemplo para a funcionalidade de moderação.
static char* moderator_words[MAX_MODERATOR_WORDS];
static int num_moderator_words = 0;

// --- Estruturas para o histórico de mensagens ---
static char* message_history[HISTORY_SIZE];
static int history_count = 0;
static int history_start = 0;
static pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Funções de Gerenciamento de Clientes (com proteção de mutex) ---

void add_client(int socket, const char* nickname) {
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        clients[client_count].socket = socket;
        strncpy(clients[client_count].nickname, nickname, BUFFER_SIZE - 1);
        clients[client_count].nickname[BUFFER_SIZE - 1] = '\0';
        client_count++;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            clients[i] = clients[client_count - 1]; 
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Adiciona uma mensagem ao buffer circular do histórico
void add_to_history(const char* message) {
    pthread_mutex_lock(&history_mutex);
    
    // Libera a memória da mensagem mais antiga se o buffer estiver cheio
    if (history_count == HISTORY_SIZE) {
        free(message_history[history_start]);
        history_start = (history_start + 1) % HISTORY_SIZE;
    } else {
        history_count++;
    }

    // Calcula o índice para a nova mensagem
    int new_index = (history_start + history_count - 1) % HISTORY_SIZE;
    message_history[new_index] = strdup(message);

    pthread_mutex_unlock(&history_mutex);
}

// Envia o histórico de mensagens para um cliente recém-conectado
void send_history(int client_socket) {
    // 1. Criar armazenamento local para a cópia do histórico
    char* local_history[HISTORY_SIZE];
    int local_count = 0;

    // Inicializa o array local para evitar ponteiros "lixo"
    for (int i = 0; i < HISTORY_SIZE; i++) {
        local_history[i] = NULL;
    }

    // 2. Iniciar seção crítica para copiar os dados
    pthread_mutex_lock(&history_mutex);

    for (int i = 0; i < history_count; i++) {
        int index = (history_start + i) % HISTORY_SIZE;
        if (message_history[index] != NULL) {
            // strdup aloca nova memória e copia o conteúdo da string.
            // Isso é crucial para evitar que a string original seja liberada por outra thread enquanto estamos usando a cópia.
            local_history[local_count] = strdup(message_history[index]);
            
            // Verifica se strdup teve sucesso antes de incrementar
            if (local_history[local_count] != NULL) {
                local_count++;
            }
        }
    }

    // 3. Encerrar a seção crítica. Agora estamos seguros para fazer I/O.
    pthread_mutex_unlock(&history_mutex);


    // 4. Realizar as operações de I/O de rede (lentas) fora do bloqueio do mutex
    char history_header[] = "--- Histórico das últimas mensagens ---\n";
    write(client_socket, history_header, strlen(history_header));

    for (int i = 0; i < local_count; i++) {
        write(client_socket, local_history[i], strlen(local_history[i]));
    }

    char history_footer[] = "--- Fim do histórico ---\n";
    write(client_socket, history_footer, strlen(history_footer));


    // 5. Liberar a memória que foi alocada por strdup para evitar vazamentos
    for (int i = 0; i < local_count; i++) {
        free(local_history[i]);
    }
}

void broadcast_message(const char* message, int sender_socket) {
    // Faz uma cópia local e filtra essa cópia — envia a cópia filtrada.
    char filtered_msg[(BUFFER_SIZE * 2)];
    strncpy(filtered_msg, message, sizeof(filtered_msg) - 1);
    filtered_msg[sizeof(filtered_msg) - 1] = '\0';
    filter_message(filtered_msg);

    // Mantendo o padrão de "copiar e depois enviar"
    Client local_clients[MAX_CLIENTS];
    int local_client_count;

    pthread_mutex_lock(&clients_mutex);
    local_client_count = client_count;
    memcpy(local_clients, clients, sizeof(Client) * client_count);
    pthread_mutex_unlock(&clients_mutex);

    for (int i = 0; i < local_client_count; i++) {
        if (local_clients[i].socket != sender_socket) {
            if (write(local_clients[i].socket, filtered_msg, strlen(filtered_msg)) < 0) {
                LOG_ERROR("Falha ao enviar mensagem broadcast.");
            }
        }
    }
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    char nickname[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    char message[(BUFFER_SIZE * 2) + 100];
    int read_size;

    if ((read_size = read(client_socket, nickname, sizeof(nickname) - 1)) > 0) {
        nickname[read_size] = '\0';
    } else {
        close(client_socket);
        return NULL;
    }
    
    add_client(client_socket, nickname);

    snprintf(message, sizeof(message), "[SERVER]: %s entrou no chat.\n", nickname);
    LOG_INFO(message);
    broadcast_message(message, client_socket);

    send_history(client_socket);

    while ((read_size = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[read_size] = '\0';
        
        if (strncmp(buffer, "/msg ", 5) == 0) {
            // É uma mensagem privada
            char target_nickname[BUFFER_SIZE];
            char private_msg_content[BUFFER_SIZE];
            
            if (sscanf(buffer + 5, "%s %[^\n]", target_nickname, private_msg_content) >= 1) {
                char log_request[BUFFER_SIZE * 3];
                snprintf(log_request, sizeof(log_request), "Solicitação de mensagem privada: %.20s -> %.20s: %.100s", nickname, target_nickname, private_msg_content);
                LOG_INFO(log_request);
                // Filtra o conteúdo da mensagem privada ANTES de enviá-la.
                filter_message(private_msg_content);
                 
                send_private_message(private_msg_content, nickname, target_nickname, client_socket);
            } else {
                char* usage_msg = "[SERVER]: Uso incorreto. Use: /msg <nickname> <mensagem>\n";
                write(client_socket, usage_msg, strlen(usage_msg));
                char log_error[BUFFER_SIZE * 3];
                snprintf(log_error, sizeof(log_error), "Uso incorreto de /msg por %s: %s", nickname, buffer);
                LOG_WARN(log_error);
            }
        } else {
            // É uma mensagem pública
            snprintf(message, sizeof(message), "[%s]: %s", nickname, buffer);
            filter_message(message);
            
            char log_msg[(BUFFER_SIZE * 2) + 100];
            snprintf(log_msg, sizeof(log_msg), "Mensagem recebida de %s: %s", nickname, buffer);
            LOG_INFO(log_msg);

            char filtered_message[(BUFFER_SIZE * 2) + 100];
            strncpy(filtered_message, message, sizeof(filtered_message) - 1);
            filtered_message[sizeof(filtered_message) - 1] = '\0';
            filter_message(filtered_message);

            add_to_history(filtered_message);
            broadcast_message(filtered_message, client_socket);
        }
    }

    snprintf(message, sizeof(message), "[SERVER]: %s saiu do chat.\n", nickname);
    LOG_INFO(message);
    broadcast_message(message, client_socket);

    close(client_socket);
    remove_client(client_socket);
    return NULL;
}

/**
 * @brief Carrega a lista de palavras para moderação do arquivo moderador.txt.
 * As palavras são usadas como exemplo para demonstrar a funcionalidade.
 */
void load_moderator_list() {
    FILE* file = fopen("moderador.txt", "r");
    if (file == NULL) {
        LOG_WARN("Arquivo moderador.txt não encontrado. O filtro de palavras não estará ativo.");
        return;
    }

    char line[100];
    while (fgets(line, sizeof(line), file) && num_moderator_words < MAX_MODERATOR_WORDS) {
        // Ignora linhas de comentário (que começam com #) e linhas vazias
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // Remove o caractere de nova linha (\n ou \r) do final da linha
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) > 0) {
            moderator_words[num_moderator_words] = strdup(line);
            num_moderator_words++;
        }
    }
    fclose(file);

    if (num_moderator_words > 0) {
        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg), "Filtro de moderação ativado. Carregadas %d palavras de exemplo.", num_moderator_words);
        LOG_INFO(log_msg);
    }
}

char* strcasestr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char *h, *n;
            for (h = haystack, n = needle; *h && *n; h++, n++) {
                if (tolower((unsigned char)*h) != tolower((unsigned char)*n)) {
                    break;
                }
            }
            if (!*n) {
                return (char*)haystack;
            }
        }
    }
    return NULL;
}

/**
 * @brief Procura e censura palavras da lista de moderação em uma mensagem.
 * @param message A string da mensagem a ser filtrada.
 */
void filter_message(char* message) {
    for (int i = 0; i < num_moderator_words; i++) {
        char* word_to_find = moderator_words[i];
        size_t word_len = strlen(word_to_find);
        char* found = strcasestr(message, word_to_find);
        
        while (found != NULL) {
            // Encontrou uma palavra, substitui por '*'
            for (size_t j = 0; j < word_len; j++) {
                found[j] = '*';
            }
            // Procura pela próxima ocorrência da mesma palavra
            found = strcasestr(found + 1, word_to_find);
        }
    }
}

void send_private_message(const char* message, const char* sender_nickname, const char* target_nickname, int sender_socket) {
    int target_socket = -1;
    char confirmation_msg[100];

    // Cópia thread-safe dos clientes conectados
    Client local_clients[MAX_CLIENTS];
    int local_client_count;
    pthread_mutex_lock(&clients_mutex);
    local_client_count = client_count;
    memcpy(local_clients, clients, sizeof(Client) * client_count);
    pthread_mutex_unlock(&clients_mutex);

    // Busca o destinatário
    for (int i = 0; i < local_client_count; i++) {
        if (strcmp(local_clients[i].nickname, target_nickname) == 0) {
            target_socket = local_clients[i].socket;
            break;
        }
    }

    if (target_socket != -1) {
        char private_message[BUFFER_SIZE];
        snprintf(private_message, sizeof(private_message), "[Privado de %s]: %s\n", sender_nickname, message);

        char log_msg[BUFFER_SIZE * 2];
        snprintf(log_msg, sizeof(log_msg), "Mensagem PRIVADA de %s para %s: %s", sender_nickname, target_nickname, message);
        LOG_INFO(log_msg);

        // Aplica o filtro no texto completo antes de enviar
        filter_message(private_message);

        write(target_socket, private_message, strlen(private_message));

        snprintf(confirmation_msg, sizeof(confirmation_msg), "[SERVER]: Mensagem enviada para %s.\n", target_nickname);
        write(sender_socket, confirmation_msg, strlen(confirmation_msg));
    } else {
        snprintf(confirmation_msg, sizeof(confirmation_msg), "[SERVER]: Usuário '%s' não encontrado ou offline.\n", target_nickname);
        write(sender_socket, confirmation_msg, strlen(confirmation_msg));

        char log_msg[BUFFER_SIZE];
        snprintf(log_msg, sizeof(log_msg), "Tentativa de mensagem privada FALHOU: %s tentou enviar para %s (usuário não encontrado)", sender_nickname, target_nickname);
        LOG_WARN(log_msg);
    }
}

void shutdown_handler(int signal) {
    (void)signal;
    LOG_INFO("Sinal SIGINT recebido. Iniciando desligamento...");
    g_server_running = 0;

    // Fecha o socket principal de escuta para desbloquear o accept()
    if (g_server_socket >= 0) {
        close(g_server_socket);
        g_server_socket = -1;
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Porta inválida: %d\n", port);
        return 1;
    }

    // Registra o handler para o sinal SIGINT (Ctrl+C)
    signal(SIGINT, shutdown_handler);

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    logger_init();
    load_moderator_list();
    LOG_INFO("Iniciando o servidor de chat... (Pressione Ctrl+C para encerrar)");

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        LOG_ERROR("Falha ao criar o socket.");
        return 1;
    }

    g_server_socket = server_socket;  // Atribui à variável global

    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Bind falhou.");
        close(server_socket);
        return 1;
    }

    LOG_INFO("Servidor escutando na porta especificada.");
    listen(server_socket, 5);
    LOG_INFO("Aguardando conexões de clientes...");

    while (g_server_running) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (!g_server_running) break;

        if (client_socket < 0) {
            if (g_server_running) LOG_ERROR("Accept falhou.");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg), "Nova conexão de %s:%d (socket %d)", client_ip, ntohs(client_addr.sin_port), client_socket);
        LOG_INFO(log_msg);

        pthread_t client_thread;
        int* new_socket_ptr = malloc(sizeof(int));
        *new_socket_ptr = client_socket;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&client_thread, &attr, handle_client, (void*)new_socket_ptr) < 0) {
            LOG_ERROR("Falha ao criar a thread do cliente.");
            free(new_socket_ptr);
            close(client_socket);
        }
        pthread_attr_destroy(&attr);
    }

    LOG_INFO("Servidor: notificando todos os clientes sobre o encerramento...");

    const char* shutdown_msg = "[SERVER]: O servidor foi encerrado. Você será desconectado.\n";
    broadcast_message(shutdown_msg, -1);
    sleep(1);

    // Fecha todos os sockets de cliente restantes
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        close(clients[i].socket);
    }
    client_count = 0;
    pthread_mutex_unlock(&clients_mutex);

    close(server_socket);
    logger_destroy();
    printf("\nServidor finalizado com sucesso.\n");
    return 0;
}