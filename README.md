# Trabalho Final - Linguagem de Programação II
**Aluna:** Georgiana Maria Braga Graça
## Tema A: Servidor de Chat Multiusuário (TCP)

Este repositório contém a implementação do trabalho final da disciplina de Programação Concorrente, focado no desenvolvimento de um sistema cliente/servidor em C utilizando conceitos como threads, mutex, variáveis de condição e sockets.

---

## 🎥 Vídeo de Demonstração

[![Assistir Demonstração - Servidor de Chat Multiusuário](https://img.youtube.com/vi/_E0mlEIwe-o/0.jpg)](https://www.youtube.com/watch?v=_E0mlEIwe-o)

### Funcionalidades Implementadas

* **Biblioteca `libtslog`**: Uma biblioteca de logging em C, modular e totalmente thread-safe.
* **API Clara e Simples**: A biblioteca expõe uma interface com três funções principais: `logger_init()`, `logger_log()` e `logger_destroy()`.
* **Arquitetura Produtor-Consumidor**: O logger utiliza uma thread dedicada para as operações de E/S (escrita no console), desacoplando as threads de trabalho da escrita de logs e melhorando a performance.
* **Teste de Concorrência**: Um programa CLI (`test_logging`) foi desenvolvido para validar a segurança da biblioteca, simulando múltiplas threads que geram logs concorrentemente.
* **Build System**: Um `Makefile` foi configurado para compilar todo o projeto de forma automatizada.

**O sistema de chat implementado possui as seguintes funcionalidades:**

* **Chat em Tempo Real:** Múltiplos clientes podem se conectar e conversar publicamente.
* **Notificações de Conexão:** Todos os usuários são notificados quando um novo participante entra ou sai do chat.
* **Histórico de Mensagens:** Novos clientes recebem as últimas 15 mensagens da conversa ao se conectarem.
* **Mensagens Privadas:** Os usuários podem enviar mensagens diretas para outros participantes usando o comando `/msg`.
* **Moderação de Conteúdo:** Um filtro de palavras dinâmico, carregado a partir do arquivo `moderador.txt`, censura conteúdos predefinidos nas mensagens públicas.
* **Servidor Concorrente:** O servidor utiliza uma thread por cliente e protege as estruturas de dados compartilhadas com mutexes.
* **Logging de Eventos:** O servidor registra todas as ações importantes (conexões, mensagens, erros) usando uma biblioteca de log thread-safe.

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

## Como Compilar e Executar

O projeto utiliza `make` para a compilação.

1.  **Limpar builds anteriores (opcional):**
    ```bash
    make clean
    ```

2.  **Compilar o projeto:**
    ```bash
    make
    ```

3.  **Executar o Chat:**
O método recomendado é usar o script `start_server.sh`.

**Método Automatizado (Recomendado):**
O script irá compilar o projeto, iniciar o servidor em background e exibir os comandos para você conectar os clientes.
```bash
# Dê permissão de execução (apenas na primeira vez)
chmod +x start_server.sh

# Execute o script
./start_server.sh
```
Após rodar o script, abra novos terminais e use os comandos de cliente que aparecerão na tela.

**Método Manual:**
1.  **Inicie o servidor** em um terminal (ex: na porta 8080):
    ```bash
    ./server 8080
    ```
2.  **Inicie quantos clientes** desejar em **novos terminais**:
    ```bash
    ./client <SeuNome> 127.0.0.1 8080
    ```

### 3. Comandos do Chat

* **Mensagem Pública:** Simplesmente digite sua mensagem e pressione Enter.
    ```
    Olá a todos!
    ```
* **Mensagem Privada:** Use o formato `/msg <nickname> <mensagem>`.
    ```
    /msg Ana Reunião às 15h, não se atrase.
    ```
---