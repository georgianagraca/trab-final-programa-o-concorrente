#ifndef TSLOG_H
#define TSLOG_H

#include <stdio.h>
#include <stdlib.h>

// níveis de log
typedef enum {
    INFO,
    WARNING,
    ERROR
} LogLevel;

/**
 * @brief Inicializa o sistema de logging.
 *
 * Deve ser chamada uma única vez no início do programa.
 * Esta função cria uma thread em background para processar e escrever os logs.
 */
void logger_init();

/**
 * @brief Finaliza o sistema de logging.
 *
 * Deve ser chamada uma única vez no final do programa para garantir
 * que todos os logs pendentes sejam escritos e os recursos liberados.
 */
void logger_destroy();

/**
 * @brief Adiciona uma mensagem à fila de log.
 *
 * Esta função é thread-safe e pode ser chamada de múltiplas threads.
 * @param level O nível da mensagem de log (INFO, WARNING, ERROR).
 * @param message A mensagem a ser logada.
 */
void logger_log(LogLevel level, const char* message);

#define LOG_INFO(msg) logger_log(INFO, msg)
#define LOG_WARN(msg) logger_log(WARNING, msg)
#define LOG_ERROR(msg) logger_log(ERROR, msg)

#endif 