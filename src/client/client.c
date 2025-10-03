#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048

// Thread para receber mensagens do servidor
void* receive_handler(void* socket_desc) {
    int sock = *(int*)socket_desc;
    char server_reply[BUFFER_SIZE];
    int read_size;

    // Loop para ler e imprimir mensagens do servidor
    while ((read_size = read(sock, server_reply, sizeof(server_reply) - 1)) > 0) {
        server_reply[read_size] = '\0';
        printf("%s", server_reply);
        fflush(stdout); // Garante que a mensagem seja impressa imediatamente
    }

    if (read_size == 0) {
        printf("\nServidor desconectado. Pressione Enter para sair.\n");
    } else {
        perror("recv falhou");
    }
    
    // Sinaliza para a thread principal sair
    close(sock);
    exit(0);
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <seu_nome> <ip_servidor> <porta>\n", argv[0]);
        return 1;
    }
    
    char* nickname = argv[1];
    char* server_ip = argv[2];
    int port = atoi(argv[3]);

    int sock;
    struct sockaddr_in server;


    // 1. Criar Socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Não foi possível criar o socket");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr(server_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // 2. Conectar ao Servidor
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Conexão falhou");
        return 1;
    }
    printf("Conectado ao servidor! Você pode começar a digitar.\n");

    // 3. Criar a thread para receber mensagens
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, (void*)&sock) < 0) {
        perror("Não foi possível criar a thread de recebimento");
        return 1;
    }

    // 4. Thread principal lê o input do usuário e envia
    while (1) {
        char buffer[BUFFER_SIZE - 100]; // Deixa espaço para o nickname
        char formatted_message[BUFFER_SIZE];

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            // Ocorreu um erro ou o input foi fechado (Ctrl+D)
            break;
        }

        snprintf(formatted_message, sizeof(formatted_message), "[%s]: %s", nickname, buffer);

        if (write(sock, formatted_message, strlen(formatted_message)) < 0) {
            perror("Envio falhou");
            break;
        }
    }

    close(sock);
    return 0;
}