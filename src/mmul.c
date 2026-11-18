/*
 * CI1316 — Programação Paralela — 2º semestre de 2026
 * Trabalho 2: Multiplicação de matrizes com MPI
 *
 * Versão A: multiplicação paralela simples (distribuição de faixas de
 *           linhas de A; cada processo guarda B inteira).
 * Versão B: multiplicação blocada com ladrilhos (tiling) w x w, laços
 *           externos ii -> kk -> jj, para reaproveitar blocos no cache.
 *
 * Uso:
 *   mmul Nla M Ncb               -> versão A (modo medição)
 *   mmul Nla M Ncb -w blockWidth -> versão B (modo medição)
 *   mmul Nla M Ncb -v [-w w]     -> modo verificador (compara com serial)
 *
 * Autores:
 *   Gustavo Gabriel Ripka  <GRR20203935>
 *   Arthur Barreto Godoi   <GRR20224377>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

#include "chrono.c" /* biblioteca de cronometragem do prof. Zola */

/* ------------------------------------------------------------------ */
/* Funções da especificação do trabalho                                */
/* ------------------------------------------------------------------ */

/*
 * Inicializa uma matriz de doubles com valores inteiros em [-4..4].
 * Na versão VETORIZADA, cada elemento recebe rand()%9 - 4.
 */
static void gera_matriz(double *mat, size_t n)
{
    const int nValores = 9;

    for (size_t i = 0; i < n; i++) {
        int r = rand() % nValores;
        mat[i] = (double)(r - 4);
    }
}

/*
 * Confere se C é igual a A*B.
 * Calcula Caux = A*B de forma sequencial, com precisão exata, e compara
 * elemento por elemento. Na primeira diferença imprime os índices e os
 * valores divergentes. Retorna o número de elementos errados.
 */
static size_t verifica_resultado(int Nla, int M, int Ncb,
                                 const double *A, const double *B,
                                 const double *C)
{
    size_t nErros = 0;
    double *Caux = (double *)calloc((size_t)Nla * (size_t)Ncb, sizeof(double));

    if (Caux == NULL) {
        fprintf(stderr, "verifica_resultado: sem memoria para Caux\n");
        exit(1);
    }

    /* referência sequencial, ordem i -> k -> j */
    for (int i = 0; i < Nla; i++) {
        double *Ci = Caux + (size_t)i * (size_t)Ncb;
        for (int k = 0; k < M; k++) {
            double a = A[(size_t)i * (size_t)M + (size_t)k];
            const double *Bk = B + (size_t)k * (size_t)Ncb;
            for (int j = 0; j < Ncb; j++)
                Ci[j] += a * Bk[j];
        }
    }

    for (int i = 0; i < Nla; i++) {
        for (int j = 0; j < Ncb; j++) {
            size_t idx = (size_t)i * (size_t)Ncb + (size_t)j;
            if (C[idx] != Caux[idx]) {
                if (nErros == 0) {
                    fprintf(stderr, "verifica_resultado: 1o erro em (%d,%d): "
                                    "C=%.0f Caux=%.0f\n",
                            i, j, C[idx], Caux[idx]);
                }
                nErros++;
            }
        }
    }

    free(Caux);
    return nErros;
}

/* ------------------------------------------------------------------ */
/* Multiplicação local (faixa de nl linhas de A)                       */
/* ------------------------------------------------------------------ */

/*
 * Versão A — kernel simples e direto:
 * para cada linha local i de A, varre B inteira acumulando em C.
 * (sem blocagem; é o kernel "normal" pedido na versão A)
 */
static void mmul_simples(double *Cl, const double *Al, const double *B,
                         int nl, int M, int Ncb)
{
    for (int i = 0; i < nl; i++) {
        double *Ci = Cl + (size_t)i * (size_t)Ncb;
        const double *Ai = Al + (size_t)i * (size_t)M;

        for (int j = 0; j < Ncb; j++)
            Ci[j] = 0.0;

        for (int k = 0; k < M; k++) {
            double a = Ai[k];
            const double *Bk = B + (size_t)k * (size_t)Ncb;

            for (int j = 0; j < Ncb; j++)
                Ci[j] += a * Bk[j];
        }
    }
}

/*
 * Versão B — kernel blocada (tiling):
 * os laços externos percorrem blocos ii (linhas de A/C), kk (colunas de
 * A / linhas de B) e jj (colunas de B/C). Para cada trio de blocos, o
 * bloco w x w de B é reaproveitado por todas as w linhas do bloco de A,
 * permanecendo no cache. Só o último bloco de linhas da faixa local pode
 * ser menor que w.
 */
static void mmul_blocada(double *Cl, const double *Al, const double *B,
                         int nl, int M, int Ncb, int w)
{
    for (int ii = 0; ii < nl; ii += w) {
        int ni = (nl - ii < w) ? (nl - ii) : w;

        for (int kk = 0; kk < M; kk += w) {
            int nk = (M - kk < w) ? (M - kk) : w;

            for (int jj = 0; jj < Ncb; jj += w) {
                int nj = (Ncb - jj < w) ? (Ncb - jj) : w;

                for (int i = 0; i < ni; i++) {
                    double *Ci = Cl + (size_t)(ii + i) * (size_t)Ncb + (size_t)jj;
                    const double *Ai = Al + (size_t)(ii + i) * (size_t)M + (size_t)kk;

                    for (int k = 0; k < nk; k++) {
                        double a = Ai[k];
                        const double *Bk = B + (size_t)(kk + k) * (size_t)Ncb + (size_t)jj;

                        for (int j = 0; j < nj; j++)
                            Ci[j] += a * Bk[j];
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    int rank, nprocs;
    int Nla = 0, M = 0, Ncb = 0;
    int modoVerifica = 0;
    int usarBlocada = 0;
    int w = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /* a) cada processo imprime o seu rank e o nome do host */
    {
        char nome[MPI_MAX_PROCESSOR_NAME];
        int tam;

        MPI_Get_processor_name(nome, &tam);
        printf("Rank %d de %d rodando em %s\n", rank, nprocs, nome);
        fflush(stdout);
    }

    if (argc < 4) {
        if (rank == 0)
            fprintf(stderr,
                    "uso: %s Nla M Ncb [-v] [-w blockWidth]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    Nla = atoi(argv[1]);
    M   = atoi(argv[2]);
    Ncb = atoi(argv[3]);

    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            modoVerifica = 1;
        } else if (strcmp(argv[i], "-w") == 0) {
            if (i + 1 >= argc) {
                if (rank == 0)
                    fprintf(stderr, "erro: -w exige o valor de blockWidth\n");
                MPI_Finalize();
                return 1;
            }
            usarBlocada = 1;
            w = atoi(argv[++i]);
        } else {
            if (rank == 0)
                fprintf(stderr, "erro: argumento desconhecido '%s'\n", argv[i]);
            MPI_Finalize();
            return 1;
        }
    }

    if (Nla <= 0 || M <= 0 || Ncb <= 0) {
        if (rank == 0)
            fprintf(stderr, "erro: Nla, M e Ncb devem ser positivos\n");
        MPI_Finalize();
        return 1;
    }

    /* Restrições do exercício para a versão B (simplificada):
     * colunas múltiplas de 8 (linhas alinhadas), w múltiplo de 8 e
     * divisor de M e Ncb, e Nla divisível por nprocs. */
    if (usarBlocada) {
        if (M % 8 || Ncb % 8 || Nla % nprocs || w % 8 || M % w || Ncb % w) {
            if (rank == 0)
                fprintf(stderr,
                        "erro: versao B exige M e Ncb multiplos de 8 e de w, "
                        "w multiplo de 8 e Nla divisivel por nprocs "
                        "(Nla=%d M=%d Ncb=%d w=%d nprocs=%d)\n",
                        Nla, M, Ncb, w, nprocs);
            MPI_Finalize();
            return 1;
        }
    }

    /* ---------------- distribuição de memória ---------------- */

    int linhasBase = Nla / nprocs;
    int linhasExtra = Nla % nprocs;
    int nl = linhasBase + (rank < linhasExtra ? 1 : 0);

    int *contagens = malloc((size_t)nprocs * sizeof(int));
    int *desloc    = malloc((size_t)nprocs * sizeof(int));

    if (contagens == NULL || desloc == NULL) {
        fprintf(stderr, "rank %d: sem memoria\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int acc = 0;
    for (int p = 0; p < nprocs; p++) {
        int linhasP = linhasBase + (p < linhasExtra ? 1 : 0);

        contagens[p] = linhasP * M;  /* elementos de A (linhas x M) */
        desloc[p] = acc;
        acc += contagens[p];
    }

    double *A = NULL, *B = NULL, *C = NULL;
    if (rank == 0) {
        A = malloc((size_t)Nla * (size_t)M * sizeof(double));
        B = malloc((size_t)M   * (size_t)Ncb * sizeof(double));
        C = malloc((size_t)Nla * (size_t)Ncb * sizeof(double));

        if (A == NULL || B == NULL || C == NULL) {
            fprintf(stderr, "rank 0: sem memoria para as matrizes\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        /* inicialização apenas no nodo 0; srand(1) antes da 1a chamada */
        srand(1);
        gera_matriz(A, (size_t)Nla * (size_t)M);
        gera_matriz(B, (size_t)M   * (size_t)Ncb);
    }

    double *Al = malloc((size_t)nl  * (size_t)M   * sizeof(double));
    double *Bl = malloc((size_t)M   * (size_t)Ncb * sizeof(double));
    double *Cl = malloc((size_t)nl  * (size_t)Ncb * sizeof(double));

    if (Al == NULL || Bl == NULL || Cl == NULL) {
        fprintf(stderr, "rank %d: sem memoria para as faixas locais\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    memset(Cl, 0, (size_t)nl * (size_t)Ncb * sizeof(double));

    /* ---------------- medição ---------------- */

    chronometer_t cronoTotal, cronoComp;
    double tTotal = 0.0, tComp = 0.0;

    chrono_reset(&cronoTotal);
    chrono_reset(&cronoComp);

    MPI_Barrier(MPI_COMM_WORLD);
    chrono_start(&cronoTotal);

    /* o rank 0 mantém B em Bl para que a mesma rotina coletiva funcione
     * também quando nprocs == 1 */
    if (rank == 0)
        memcpy(Bl, B, (size_t)M * (size_t)Ncb * sizeof(double));

    /* ---------------- comunicação inicial ---------------- */

    if (usarBlocada) {
        /* versão B: sobrepõe o envio de B e a distribuição de A usando
         * operações coletivas não bloqueantes (MPI-3) */
        MPI_Request pedidos[2];

        MPI_Ibcast(Bl, M * Ncb, MPI_DOUBLE, 0, MPI_COMM_WORLD, &pedidos[0]);
        MPI_Iscatterv(A, contagens, desloc, MPI_DOUBLE,
                      Al, contagens[rank], MPI_DOUBLE,
                      0, MPI_COMM_WORLD, &pedidos[1]);
        MPI_Waitall(2, pedidos, MPI_STATUSES_IGNORE);
    } else {
        MPI_Bcast(Bl, M * Ncb, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Scatterv(A, contagens, desloc, MPI_DOUBLE,
                     Al, contagens[rank], MPI_DOUBLE,
                     0, MPI_COMM_WORLD);
    }

    /* ---------------- computação ---------------- */

    chrono_start(&cronoComp);

    if (usarBlocada)
        mmul_blocada(Cl, Al, Bl, nl, M, Ncb, w);
    else
        mmul_simples(Cl, Al, Bl, nl, M, Ncb);

    chrono_stop(&cronoComp);

    /* ---------------- comunicação final (reunião de C) ---------------- */

    int *contagensC = malloc((size_t)nprocs * sizeof(int));
    int *deslocC    = malloc((size_t)nprocs * sizeof(int));

    acc = 0;
    for (int p = 0; p < nprocs; p++) {
        int linhasP = linhasBase + (p < linhasExtra ? 1 : 0);
        contagensC[p] = linhasP * Ncb;
        deslocC[p] = acc;
        acc += contagensC[p];
    }

    MPI_Gatherv(Cl, contagensC[rank], MPI_DOUBLE,
                C, contagensC, deslocC, MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    chrono_stop(&cronoTotal);

    /* tempos máximos entre os processos (o mais lento dita o total) */
    long long nsTotal = chrono_gettotal(&cronoTotal);
    long long nsComp  = chrono_gettotal(&cronoComp);
    long long maxTotal = 0, maxComp = 0;

    MPI_Reduce(&nsTotal, &maxTotal, 1, MPI_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&nsComp,  &maxComp,  1, MPI_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        tTotal = (double)maxTotal / 1e9;
        tComp  = (double)maxComp  / 1e9;

        double tComunic = tTotal - tComp;
        double operacoes = 2.0 * (double)Nla * (double)M * (double)Ncb;
        double gflops = (tTotal > 0.0) ? operacoes / tTotal / 1e9 : 0.0;

        printf("\nversao %s (%d processos MPI)\n",
               usarBlocada ? "B (blocada)" : "A (simples)", nprocs);
        if (usarBlocada)
            printf("blockWidth w = %d\n", w);
        printf("Tempo total:        %.6f s\n", tTotal);
        printf("Tempo de computacao:%.6f s\n", tComp);
        printf("Tempo de comunicacao: %.6f s\n", tComunic);
        printf("VAZAO: %.3f GFLOPS\n", gflops);
        fflush(stdout);
    }

    /* ---------------- modo verificador ---------------- */

    if (modoVerifica && rank == 0) {
        size_t nErros = verifica_resultado(Nla, M, Ncb, A, B, C);

        if (nErros == 0)
            printf("verifica ok: resultado paralelo identico ao sequencial\n");
        else
            printf("verifica erro: %zu elementos errados\n", nErros);
        fflush(stdout);
    }

    /* ---------------- liberação ---------------- */

    free(contagens);
    free(desloc);
    free(contagensC);
    free(deslocC);
    free(Al);
    free(Bl);
    free(Cl);
    if (rank == 0) {
        free(A);
        free(B);
        free(C);
    }

    MPI_Finalize();
    return 0;
}
