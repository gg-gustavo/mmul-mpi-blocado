# Trabalho 2 - CI1316 - Multiplicação de matrizes com MPI
# Autores: Gustavo Gabriel Ripka <GRR20203935>, Arthur Barreto Godoi <GRR20224377>
#
# uso:
#   make            - compila o programa (binario em src/mmul)
#   make test       - testes rapidos de correcao (-v), versoes A e B
#   make testbig    - testes de correcao com tamanhos maiores
#   make clean      - remove binario e artefatos

CC      = mpicc
CFLAGS  = -O3 -Wall -Wextra
BIN     = src/mmul

all: $(BIN)

$(BIN): src/mmul.c src/chrono.c
	$(CC) $(CFLAGS) -o $@ src/mmul.c -lm

test: $(BIN)
	@echo "== versao A, 1 processo =="
	mpirun -np 1 $(BIN) 64 64 64 -v | grep -e verifica -e VAZAO
	@echo "== versao A, 2 processos =="
	mpirun -np 2 $(BIN) 64 64 64 -v | grep -e verifica -e VAZAO
	@echo "== versao A, 4 processos =="
	mpirun -np 4 $(BIN) 64 64 64 -v | grep -e verifica -e VAZAO
	@echo "== versao B, 4 processos, w=16 =="
	mpirun -np 4 $(BIN) 64 64 64 -v -w 16 | grep -e verifica -e VAZAO

testbig: $(BIN)
	@echo "== versao A, 4 processos, 256x256x256 =="
	mpirun -np 4 $(BIN) 256 256 256 -v | grep -e verifica -e VAZAO
	@echo "== versao B, 4 processos, 256x256x256 w=64 =="
	mpirun -np 4 $(BIN) 256 256 256 -v -w 64 | grep -e verifica -e VAZAO

clean:
	rm -f $(BIN) *.aux *.log *.out

.PHONY: all test testbig clean
