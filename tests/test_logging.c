#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>   
#include "../src/libtslog/tslog.h"

#define NUM_THREADS 10
#define MESSAGES_PER_THREAD 100

/**
 * @brief Função executada por cada thread para gerar logs concorrentemente.
 * @param arg O ID da thread, convertido para um ponteiro void*.
 * @return NULL
 */
void* log_worker(void* arg) {
    long thread_id = (long)arg;
    char message_buffer[256];

    for (int i = 0; i < MESSAGES_PER_THREAD; ++i) {
        snprintf(message_buffer, sizeof(message_buffer),
                 "Mensagem %d da thread %ld", i, thread_id);
        
        // Gera logs de diferentes níveis para teste
        if (i % 3 == 0) {
            LOG_INFO(message_buffer);
        } else if (i % 3 == 1) {
            LOG_WARN(message_buffer);
        } else {
            LOG_ERROR(message_buffer);
        }
        
        // pausa para simular trabalho e permitir a troca de contexto
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 5 * 1000 * 1000;
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main() {
    printf("Iniciando teste de log concorrente...\n");
    
    logger_init();

    pthread_t threads[NUM_THREADS];

    // Cria e inicia as threads de trabalho
    for (long i = 0; i < NUM_THREADS; ++i) {
        if (pthread_create(&threads[i], NULL, log_worker, (void*)(i + 1)) != 0) {
            perror("Falha ao criar thread");
            return 1;
        }
    }

    // Espera que todas as threads de trabalho terminem sua execução
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Falha ao juntar thread");
            return 1;
        }
    }
    
    printf("Teste de log concorrente finalizado. Aguardando logs finais...\n");
    
    logger_destroy();

    return 0;
}
