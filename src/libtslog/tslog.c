#define _POSIX_C_SOURCE 200809L 
#include <string.h>             

#include "tslog.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>  

// Estrutura para uma entrada de log na fila
typedef struct LogEntry {
    LogLevel level;
    char* message;
    struct LogEntry* next;
} LogEntry;

// Fila thread-safe para as entradas de log
typedef struct {
    LogEntry *head, *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} LogQueue;

// Estrutura principal do logger
typedef struct {
    LogQueue queue;
    pthread_t writer_thread;
    int running;
} Logger;

// Instância global estática do logger
static Logger* g_logger = NULL;

// --- Implementação da Fila ---
static void queue_init(LogQueue* q) {
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

static void queue_destroy(LogQueue* q) {
    LogEntry* current = q->head;
    while (current != NULL) {
        LogEntry* next = current->next;
        free(current->message);
        free(current);
        current = next;
    }
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

static void queue_push(LogQueue* q, LogLevel level, const char* message) {
    LogEntry* new_entry = (LogEntry*)malloc(sizeof(LogEntry));
    if (!new_entry) {
        perror("Falha ao alocar LogEntry");
        return;
    }

    new_entry->message = strdup(message);
    if (!new_entry->message) {
        perror("Falha ao duplicar mensagem");
        free(new_entry);
        return;
    }

    new_entry->level = level;
    new_entry->next = NULL;

    pthread_mutex_lock(&q->mutex);
    if (q->tail == NULL) {
        q->head = q->tail = new_entry;
    } else {
        q->tail->next = new_entry;
        q->tail = new_entry;
    }
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

static LogEntry* queue_pop(LogQueue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->head == NULL && g_logger->running) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }

    if (q->head == NULL) { // Logger está finalizando e a fila está vazia
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    LogEntry* entry = q->head;
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    pthread_mutex_unlock(&q->mutex);
    return entry;
}

// --- Funções do Logger ---
static const char* level_to_string(LogLevel level) {
    switch (level) {
        case INFO:    return "INFO";
        case WARNING: return "WARNING";
        case ERROR:   return "ERROR";
    }
    return "UNKNOWN";
}

// Função executada pela thread de escrita
static void* writer_thread_func(void* arg) {
    (void)arg; // Evita warning de argumento não usado
    while (g_logger->running || g_logger->queue.head != NULL) {
        LogEntry* entry = queue_pop(&g_logger->queue);
        if (entry == NULL) {
            continue; // Continua para verificar a condição de parada novamente
        }

        char time_buf[100];
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm tm_info;
        localtime_r(&tv.tv_sec, &tm_info);
        strftime(time_buf, sizeof(time_buf) - 1, "%Y-%m-%d %H:%M:%S", &tm_info);
        
        // Imprime o log formatado no stdout
        printf("[%s.%03ld] [%ld] [%s] %s\n",
               time_buf, tv.tv_usec / 1000,
               (long)pthread_self(), // ID da thread
               level_to_string(entry->level),
               entry->message);
        fflush(stdout);

        free(entry->message);
        free(entry);
    }
    return NULL;
}

void logger_init() {
    if (g_logger != NULL) return; // Já inicializado

    g_logger = (Logger*)malloc(sizeof(Logger));
    g_logger->running = 1;
    queue_init(&g_logger->queue);
    pthread_create(&g_logger->writer_thread, NULL, writer_thread_func, NULL);
}

void logger_destroy() {
    if (g_logger == NULL) return;

    g_logger->running = 0;
    
    // Acorda a thread de escrita para que ela possa verificar a flag 'running' e sair
    pthread_mutex_lock(&g_logger->queue.mutex);
    pthread_cond_broadcast(&g_logger->queue.cond);
    pthread_mutex_unlock(&g_logger->queue.mutex);

    pthread_join(g_logger->writer_thread, NULL);

    queue_destroy(&g_logger->queue);
    free(g_logger);
    g_logger = NULL;
}

void logger_log(LogLevel level, const char* message) {
    if (g_logger == NULL || !g_logger->running) {
        fprintf(stderr, "Aviso: logger_log chamado antes de logger_init ou após logger_destroy.\n");
        return;
    }
    queue_push(&g_logger->queue, level, message);
}