#!/bin/bash
# Trabalho 2 - CI1316 - experimentos A (simples) e B (blocada)
# Autores: Gustavo Gabriel Ripka <GRR20203935>, Arthur Barreto Godoi <GRR20224377>
#
# Uso (a partir do w00, dentro da pasta scripts/):
#   sbatch --exclusive -N <nodos> roda-exp-slurm.sh <A|B>
#
# Roda 10 vezes a configuracao pedida (matrizes 2304x960x960), com 1
# processo MPI por nodo, e grava a saida em ../resultados/.
#
#SBATCH --exclusive
#SBATCH --job-name=t2-mmul
#SBATCH --time=5:00

VER=$1                       # A ou B
NP=${SLURM_NNODES:-1}        # 1 processo MPI por nodo
W=192                        # blockWidth da versao B (prof sugere 192)

SAIDA="$SLURM_SUBMIT_DIR/../resultados/saida-exp${VER}-np${NP}-job${SLURM_JOBID}.txt"

cd "$SLURM_SUBMIT_DIR/../src" || exit 1
echo "# exp=$VER np=$NP w=$W job=$SLURM_JOBID host=$(hostname) data=$(date)" > "$SAIDA"

for j in $(seq 1 10); do
    echo "### execucao $j" >> "$SAIDA"
    if [ "$VER" = "B" ]; then
        mpirun --map-by ppr:1:node -np $NP ./mmul 2304 960 960 -w $W >> "$SAIDA" 2>&1
    else
        mpirun --map-by ppr:1:node -np $NP ./mmul 2304 960 960 >> "$SAIDA" 2>&1
    fi
done

echo FIM >> "$SAIDA"
