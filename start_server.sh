#!/bin/bash

# Limpa e compila o projeto
echo "Compilando o projeto..."
make clean && make

# Inicia o servidor em background no terminal atual
echo "Iniciando o servidor em background..."
./server 8080 &
SERVER_PID=$!

echo ""
echo "✅ Servidor iniciado com sucesso (PID: $SERVER_PID)."
echo ""
echo "👉 AGORA, ABRA NOVOS TERMINAIS E EXECUTE OS COMANDOS ABAIXO:"
echo "   ./client Ana 127.0.0.1 8080"
echo "   ./client Beto 127.0.0.1 8080"
echo "   ./client Carol 127.0.0.1 8080"
echo ""
echo "Pressione [Enter] nesta janela para finalizar o servidor e encerrar o teste."

# Aguarda o usuário pressionar Enter
read

# Mata o processo do servidor
echo "Finalizando o servidor..."
kill $SERVER_PID
echo "Servidor finalizado."