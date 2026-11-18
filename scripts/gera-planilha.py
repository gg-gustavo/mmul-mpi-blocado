#!/usr/bin/env python3
# Trabalho 2 - CI1316 - gera a planilha (CSV + detalhado) e os graficos
# a partir das saidas brutas em resultados/.
#
# Autores: Gustavo Gabriel Ripka <GRR20203935>, Arthur Barreto Godoi <GRR20224377>

import glob
import os
import re

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(BASE, "resultados")
PLAN = os.path.join(BASE, "planilha")
FIG = os.path.join(BASE, "figuras")

# execucoes oficiais (o job 19337 foi um teste previo de 2 nodos)
OFICIAIS = {
    ("A", 1): "saida-expA-np1-job19338.txt",
    ("A", 2): "saida-expA-np2-job19339.txt",
    ("A", 3): "saida-expA-np3-job19340.txt",
    ("A", 4): "saida-expA-np4-job19341.txt",
    ("B", 1): "saida-expB-np1-job19342.txt",
    ("B", 2): "saida-expB-np2-job19343.txt",
    ("B", 3): "saida-expB-np3-job19344.txt",
    ("B", 4): "saida-expB-np4-job19345.txt",
}

RX_TOTAL = re.compile(r"Tempo total:\s+([0-9.]+)")
RX_COMP = re.compile(r"Tempo de computacao:\s*([0-9.]+)")
RX_COM = re.compile(r"Tempo de comunicacao:\s*([0-9.]+)")
RX_GF = re.compile(r"VAZAO:\s+([0-9.]+)")


def le_arquivo(caminho):
    runs = []
    with open(caminho, encoding="utf-8", errors="replace") as f:
        texto = f.read()
    for bloco in texto.split("### execucao")[1:]:
        total = RX_TOTAL.search(bloco)
        comp = RX_COMP.search(bloco)
        com = RX_COM.search(bloco)
        gf = RX_GF.search(bloco)
        if total and comp and com and gf:
            runs.append({
                "total": float(total.group(1)),
                "comp": float(comp.group(1)),
                "com": float(com.group(1)),
                "gflops": float(gf.group(1)),
            })
    return runs


def media(valores):
    return sum(valores) / len(valores)


def main():
    os.makedirs(PLAN, exist_ok=True)
    os.makedirs(FIG, exist_ok=True)

    detalhado = [("versao", "nodos", "execucao", "tempo_total_s",
                  "tempo_computacao_s", "tempo_comunicacao_s", "gflops")]
    resumo = {}

    for (versao, nodos), nome in OFICIAIS.items():
        runs = le_arquivo(os.path.join(RES, nome))
        if len(runs) != 10:
            raise SystemExit(f"{nome}: esperava 10 execucoes, achei {len(runs)}")
        for i, r in enumerate(runs, 1):
            detalhado.append((versao, nodos, i, f"{r['total']:.6f}",
                              f"{r['comp']:.6f}", f"{r['com']:.6f}",
                              f"{r['gflops']:.3f}"))
        resumo[(versao, nodos)] = {
            "total": media([r["total"] for r in runs]),
            "comp": media([r["comp"] for r in runs]),
            "com": media([r["com"] for r in runs]),
            "gflops": media([r["gflops"] for r in runs]),
        }

    # ---------------- CSV detalhado ----------------
    with open(os.path.join(PLAN, "execucoes-detalhado.csv"), "w") as f:
        for linha in detalhado:
            f.write(",".join(str(x) for x in linha) + "\n")

    # ---------------- CSV resumo ----------------
    cab = ("versao;nodos;tempo_total_medio_s;tempo_computacao_medio_s;"
           "tempo_comunicacao_medio_s;gflops_medio;aceleracao_vs_np1")
    linhas = [cab]
    base = {v: resumo[(v, 1)]["total"] for v in ("A", "B")}
    for versao in ("A", "B"):
        for nodos in (1, 2, 3, 4):
            r = resumo[(versao, nodos)]
            acel = base[versao] / r["total"]
            linhas.append(f"{versao};{nodos};{r['total']:.4f};{r['comp']:.4f};"
                          f"{r['com']:.4f};{r['gflops']:.3f};{acel:.3f}")
    # comparacao B/A
    for nodos in (1, 2, 3, 4):
        ra, rb = resumo[("A", nodos)], resumo[("B", nodos)]
        linhas.append(f"B/A;{nodos};{rb['total']:.4f};{ra['total']:.4f};"
                      f"{ra['total'] / rb['total']:.3f};"
                      f"{rb['gflops']:.3f};-")
    with open(os.path.join(PLAN, "planilha-resultados.csv"), "w") as f:
        f.write("\n".join(linhas) + "\n")

    # ---------------- graficos ----------------
    nodos = [1, 2, 3, 4]

    plt.figure(figsize=(6, 4))
    for versao in ("A", "B"):
        plt.plot(nodos, [resumo[(versao, n)]["gflops"] for n in nodos],
                 marker="o", label=f"Versão {versao}")
    plt.xlabel("Número de nodos (1 processo MPI por nodo)")
    plt.ylabel("Vazão (GFLOPS)")
    plt.title("Vazão × nodos — 2304×960×960 doubles")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(FIG, "grafico-vazao.png"), dpi=150)
    plt.close()

    plt.figure(figsize=(6, 4))
    for versao in ("A", "B"):
        plt.plot(nodos, [resumo[(versao, n)]["total"] for n in nodos],
                 marker="o", label=f"Versão {versao}")
    plt.xlabel("Número de nodos")
    plt.ylabel("Tempo total médio (s)")
    plt.title("Tempo total × nodos")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(FIG, "grafico-tempo.png"), dpi=150)
    plt.close()

    plt.figure(figsize=(6, 4))
    for versao in ("A", "B"):
        acel = [base[versao] / resumo[(versao, n)]["total"] for n in nodos]
        plt.plot(nodos, acel, marker="o", label=f"Versão {versao}")
    plt.plot(nodos, nodos, linestyle="--", color="gray", label="Linear")
    plt.xlabel("Número de nodos")
    plt.ylabel("Aceleração (relativa a 1 nodo)")
    plt.title("Aceleração × nodos")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(FIG, "grafico-aceleracao.png"), dpi=150)
    plt.close()

    plt.figure(figsize=(6, 4))
    razao = [resumo[("A", n)]["total"] / resumo[("B", n)]["total"]
             for n in nodos]
    plt.bar([str(n) for n in nodos], razao, color="#4c72b0")
    plt.axhline(1.0, color="gray", linestyle="--")
    plt.xlabel("Número de nodos")
    plt.ylabel("Aceleração de B em relação a A")
    plt.title("Comparação B (blocada) / A (simples)")
    plt.grid(True, axis="y", alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(FIG, "grafico-b-vs-a.png"), dpi=150)
    plt.close()

    # ---------------- resumo na tela ----------------
    print(f"{'ver':>4} {'n':>2} {'total(s)':>10} {'comp(s)':>10} "
          f"{'com(s)':>9} {'GFLOPS':>8} {'acel':>7}")
    for versao in ("A", "B"):
        for n in nodos:
            r = resumo[(versao, n)]
            print(f"{versao:>4} {n:>2} {r['total']:>10.4f} {r['comp']:>10.4f} "
                  f"{r['com']:>9.4f} {r['gflops']:>8.3f} "
                  f"{base[versao] / r['total']:>7.3f}")
    for n in nodos:
        print(f"B/A nodos={n}: {resumo[('A', n)]['total'] / resumo[('B', n)]['total']:.3f}x")


if __name__ == "__main__":
    main()
