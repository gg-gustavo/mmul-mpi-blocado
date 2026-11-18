#!/bin/bash
# Trabalho 2 - CI1316 - captura das caracteristicas da CPU usada
# Autores: Gustavo Gabriel Ripka <GRR20203935>, Arthur Barreto Godoi <GRR20224377>
#
# Uso: sbatch --exclusive -N 1 captura-lscpu-slurm.sh
#
#SBATCH --exclusive
#SBATCH --job-name=t2-lscpu
#SBATCH --time=5:00

cd "$SLURM_SUBMIT_DIR/.." || exit 1
lscpu > lscpu-nodo.txt
lstopo --of txt > lstopo-nodo.txt 2>/dev/null || lstopo-no-graphics --of txt > lstopo-nodo.txt 2>/dev/null
if command -v lstopo >/dev/null 2>&1 || command -v lstopo-no-graphics >/dev/null 2>&1; then
    (lstopo --of png lstopo-nodo.png 2>/dev/null || lstopo-no-graphics --of png lstopo-nodo.png 2>/dev/null)
fi
hostname >> lscpu-nodo.txt
