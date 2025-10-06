#!/bin/bash
set -e
cd "$(dirname "$0")"

LOGFILE="./servidor.log"
PORT=8080

# Limpa e compila o projeto
echo "Compilando o projeto..."
make clean && make || { echo "❌ Falha na compilação."; exit 1; }

# Garante que o binário do servidor existe
if [[ ! -x ./server ]]; then
  echo "❌ Binário ./server não encontrado ou sem permissão de execução."
  exit 1
fi

# Remove log antigo
rm -f "$LOGFILE"

# Inicia o servidor em background, redirecionando stdout/stderr para o log
echo "Iniciando o servidor (em background) e redirecionando logs para $LOGFILE..."
./server $PORT > "$LOGFILE" 2>&1 &
SERVER_PID=$!

sleep 2  # Aumentei o tempo para garantir que o servidor inicialize

if ps -p $SERVER_PID > /dev/null 2>&1; then
  echo "✅ Servidor iniciado com sucesso (PID: $SERVER_PID)."
  echo "Logs sendo gravados em: $LOGFILE"
else
  echo "❌ Falha ao iniciar o servidor. Cheque $LOGFILE"
  cat "$LOGFILE"
  exit 1
fi

# Resto do script permanece igual...
echo ""
echo "👉 AGORA, ABRA NOVOS TERMINAIS E EXECUTE OS COMANDOS ABAIXO:"
echo "   ./client Ana 127.0.0.1 $PORT"
echo "   ./client Beto 127.0.0.1 $PORT"
echo "   ./client Carol 127.0.0.1 $PORT"
echo ""
echo "Pressione [Enter] nesta janela para finalizar o servidor e encerrar o teste."
echo ""

# Inicia tail em foreground para mostrar logs ao vivo
tail -n +1 -f "$LOGFILE" &
TAIL_PID=$!

# Aguarda usuário pressionar Enter
read

echo "Finalizando o servidor..."
# Envia SIGINT (o servidor tem shutdown_handler para tratar)
kill -SIGINT "$SERVER_PID" 2>/dev/null || true

# Dá um timeout para o servidor encerrar graciosamente
sleep 2

# Se ainda estiver rodando, envia SIGTERM e depois SIGKILL
if ps -p $SERVER_PID > /dev/null 2>&1; then
  echo "Servidor não finalizou com SIGINT — enviando SIGTERM..."
  kill -TERM "$SERVER_PID" 2>/dev/null || true
  sleep 2
fi
if ps -p $SERVER_PID > /dev/null 2>&1; then
  echo "Servidor ainda ativo — enviando SIGKILL..."
  kill -KILL "$SERVER_PID" 2>/dev/null || true
fi

# Finaliza tail
kill "$TAIL_PID" 2>/dev/null || true

echo "Servidor finalizado."