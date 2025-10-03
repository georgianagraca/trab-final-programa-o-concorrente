# Trabalho Final - Programação Concorrente (C)

**Aluno:** Georgiana Maria Braga Graça  
**Matrícula:** 20230021653

## Tema A: Servidor de Chat Multiusuário (TCP)

Este repositório contém a implementação do trabalho final da disciplina de Programação Concorrente, focado no desenvolvimento de um sistema cliente/servidor em C utilizando conceitos como threads, mutex, variáveis de condição e sockets.

---

## Etapa 1: Biblioteca de Logging Thread-Safe e Arquitetura

Esta primeira entrega foca na criação de uma base sólida para o projeto, com a implementação de um dos requisitos transversais mais importantes: um sistema de logging que possa ser utilizado por múltiplas threads simultaneamente sem causar condições de corrida.

### Funcionalidades Implementadas

* **Biblioteca `libtslog`**: Uma biblioteca de logging em C, modular e totalmente thread-safe.
* **API Clara e Simples**: A biblioteca expõe uma interface com três funções principais: `logger_init()`, `logger_log()` e `logger_destroy()`.
* **Arquitetura Produtor-Consumidor**: O logger utiliza uma thread dedicada para as operações de E/S (escrita no console), desacoplando as threads de trabalho da escrita de logs e melhorando a performance.
* **Teste de Concorrência**: Um programa CLI (`test_logging`) foi desenvolvido para validar a segurança da biblioteca, simulando múltiplas threads que geram logs concorrentemente.
* **Build System**: Um `Makefile` foi configurado para compilar todo o projeto de forma automatizada.

---

## Etapa 2: Protótipo CLI de Comunicação

Nesta etapa, foi desenvolvido o protótipo funcional do sistema de chat. O servidor agora é capaz de aceitar múltiplas conexões de clientes concorrentemente, e os clientes podem enviar e receber mensagens em tempo real.

### Novas Funcionalidades:
* **Servidor Concorrente (`server`):** Aceita conexões TCP e cria uma nova thread para cada cliente.
* **Cliente Multi-Threaded (`client`):** Utiliza uma thread para receber mensagens do servidor e a thread principal para enviar as mensagens do usuário.
* **Broadcast de Mensagens:** O servidor retransmite as mensagens recebidas de um cliente para todos os outros clientes conectados.
* **Script de Teste (`start_server.sh`):** Automatiza a compilação e a inicialização do ambiente de teste.

---

## Arquitetura da `libtslog`

A `libtslog` foi desenhada seguindo o padrão **Produtor-Consumidor**.

1.  **Produtores**: As threads da aplicação (no nosso caso, as threads de teste em `test_logging.c`) atuam como produtoras. Ao chamar a função `logger_log()`, elas criam uma mensagem de log e a inserem em uma fila compartilhada. Esta operação é muito rápida, pois não envolve E/S de disco ou console.
2.  **Fila Thread-Safe**: Uma fila (implementada como lista ligada) serve como o buffer compartilhado. O acesso a esta fila é controlado por um **mutex** (`pthread_mutex_t`) para garantir que apenas uma thread a modifique por vez, e por uma **variável de condição** (`pthread_cond_t`) para sincronizar o produtor e o consumidor.
3.  **Consumidor**: Uma única thread, a `writer_thread`, é criada quando `logger_init()` é chamado. Ela atua como consumidora, esperando (de forma eficiente, sem consumir CPU) por mensagens na fila. Ao ser sinalizada, ela retira uma mensagem, a formata com timestamp e ID da thread, e a imprime no console.

### Diagrama de Arquitetura (ASCII)

O fluxo de dados e a interação entre as threads podem ser visualizados no diagrama abaixo:

```
                                     +----------------------+
                                     |                      |
[ Thread de Trabalho 1 ] -- log() -->|                      |
                                     |                      |
[ Thread de Trabalho 2 ] -- log() -->|  Fila Thread-Safe    | (Protegida por Mutex
                                     | (Buffer de Msgs)     |  e Var. de Condição)
[ Thread de Trabalho N ] -- log() -->|                      |
                                     |                      |
                                     +-------+--------------+
                                             |
                                             | (Retira Mensagem)
                                             v
                                     +----------------------+
                                     |                      |
                                     |  Thread do Logger    |
                                     |  (Writer Thread)     |
                                     |                      |
                                     +-------+--------------+
                                             |
                                             | (Escreve no Console)
                                             v
                                     +----------------------+
                                     |   Saída Padrão       |
                                     |   (stdout)           |
                                     +----------------------+
```

---

### 1. Compilar o Projeto
Para compilar o servidor e o cliente, execute:
```bash
make
```
Para limpar os arquivos de compilação anteriores, você pode usar `make clean`.

### 2. Executar o Chat (Servidor e Clientes)

O método recomendado é usar o script `start_server.sh`.

**Método Automatizado (Recomendado):**
O script irá compilar o projeto, iniciar o servidor em background e exibir os comandos para você conectar os clientes.
```bash
# Dê permissão de execução (apenas na primeira vez)
chmod +x start_server.sh

# Execute o script
./start_server.sh
```
Após executar o script, abra novos terminais e use os comandos de cliente que aparecerão na tela.

**Método Manual:**
1.  **Inicie o servidor** em um terminal (ex: na porta 8080):
    ```bash
    ./server 8080
    ```
2.  **Inicie quantos clientes** desejar em **novos terminais**:
    ```bash
    ./client <SeuNome> 127.0.0.1 8080
    ```

### 3. Executar o Teste de Logging (da Etapa 1)
Se desejar executar o teste específico da biblioteca de logging, use o comando:
```bash
./test_logging
```

