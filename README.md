# Trabalho 2 — Multiplicação de matrizes com MPI

**CI1316 — Programação Paralela — 2º semestre de 2026 — UFPR/DINF**

**Autores:**
- Gustavo Gabriel Ripka — GRR20203935
- Arthur Barreto Godoi — GRR20224377

Multiplicação de matrizes de `double` com MPI, em duas versões:

- **Versão A (simples):** cada processo recebe uma faixa de linhas de A
  (`MPI_Scatterv`) e a matriz B inteira (`MPI_Bcast`), calcula a sua faixa de C
  com o kernel direto e o nodo 0 reúne o resultado (`MPI_Gatherv`).
- **Versão B (blocada):** mesma distribuição, mas o kernel local percorre a
  matriz em **ladrilhos `w × w`** com laços externos `ii → kk → jj`, de modo que
  os blocos de A, B e C permaneçam no cache. A comunicação inicial usa
  operações coletivas **não bloqueantes** (`MPI_Ibcast` + `MPI_Iscatterv`)
  para sobrepor o envio de B com a distribuição de A.

## Estrutura de diretórios

```
trab2/
├── README.md            este arquivo
├── Makefile             compilação e testes (make, make test, make testbig)
├── relatorio.pdf        relatório final (tabelas, gráficos, discussão)
├── relatorio.tex        fonte do relatório
├── src/
│   ├── mmul.c           implementação (versões A e B + verificação)
│   └── chrono.c         biblioteca de medição de tempo (do prof. Zola)
├── scripts/             scripts SLURM dos experimentos (submeter com sbatch)
├── planilha/            resultados tabulados (ODS + CSV)
├── resultados/          saídas brutas dos jobs SLURM (evidência, com Job IDs)
├── figuras/             gráficos gerados (também incluídos no relatório)
└── especificacao/       arquivos da especificação original do trabalho
```

## Como compilar

```bash
make
```

Gera o binário `src/mmul`.

## Como testar a correção

```bash
make test        # versões A e B, com -v, matrizes pequenas
make testbig     # 256x256x256 com 4 processos
```

O modo verificador (`-v`) roda uma versão sequencial do algoritmo e compara o
resultado paralelo com precisão exata (`verifica_resultado`), imprimindo o
número de elementos divergentes.

## Uso

```bash
# versao A (medicao)
mpirun -np 4 src/mmul 2304 960 960

# versao B (medicao), blocos 192x192
mpirun -np 4 src/mmul 2304 960 960 -w 192

# verificacao (roda no w00, sem slurm)
mpirun -np 4 src/mmul 512 512 512 -v -w 64
```

Para cada execução o programa imprime, por processo, o rank e o nome do host,
e no nodo 0 o tempo total, o tempo de computação, o tempo de comunicação
(diferença) e a vazão em GFLOPS.

## Experiências no cluster

Scripts em `scripts/` (cada um roda 10 vezes a configuração e grava a saída em
`resultados/`):

```bash
sbatch --exclusive -N 4 scripts/roda-expA-4n-slurm.sh
```

Medições oficiais: A = 2304 × 960, B = 960 × 960, C = 2304 × 960, 1 a 4 nodos
exclusivos, 1 processo MPI por nodo, média de 10 execuções.

## Resultados obtidos (cluster Xeon, fila Slurm)

Médias de 10 execuções por configuração, 4,247 GFLOP por execução:

| nodos | A: tempo (s) | A: GFLOPS | A: acel. | B: tempo (s) | B: GFLOPS | B: acel. | B/A |
|------:|-------------:|----------:|---------:|-------------:|----------:|---------:|----:|
| 1 | 5,1723 | 0,821 | 1,000 | 2,4188 | 1,756 | 1,000 | 2,138× |
| 2 | 2,8945 | 1,468 | 1,787 | 1,4068 | 3,019 | 1,719 | 2,058× |
| 3 | 2,0232 | 2,099 | 2,556 | 1,1371 | 3,735 | 2,127 | 1,779× |
| 4 | 1,6231 | 2,617 | 3,187 | 1,0086 | 4,211 | 2,398 | 1,609× |

Versão B (ladrilhos 192×192) foi sempre mais rápida; a aceleração total é
limitada pela comunicação na rede Gigabit (41% do tempo com 4 nodos na B).
Evidências: `resultados/saida-exp*-job*.txt` (jobs 19338–19345) e
`resultados/verificacao-w00.txt` (modo `-v`, resultado idêntico ao
sequencial). Análise e gráficos: `scripts/gera-planilha.py` e
`scripts/gera-planilha-ods.py`.

## Verificação no cluster

```bash
# no w00, sem slurm
mpirun -np 4 ./mmul 2304 960 960 -v -w 192
# -> verifica ok: resultado paralelo identico ao sequencial
```

