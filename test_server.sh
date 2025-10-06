#!/bin/bash
echo "Teste básico do servidor..."

# Compila
make clean && make

# Inicia servidor em background
./server 8080 > test.log 2>&1 &
PID=$!

sleep 2

# Verifica se está rodando
if ps -p $PID > /dev/null; then
    echo "✅ Servidor iniciou (PID: $PID)"
    
    # Testa conexão
    echo "Testando conexão..."
    nc -z localhost 8080 && echo "✅ Porta 8080 respondendo" || echo "❌ Porta 8080 não respondendo"
    
    # Para o servidor
    kill $PID
else
    echo "❌ Servidor não iniciou"
    cat test.log
fi