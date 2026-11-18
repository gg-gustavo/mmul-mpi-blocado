#!/usr/bin/env python3
# Trabalho 2 - CI1316 - gera planilha-resultados.ods (3 folhas) a partir dos
# CSVs produzidos por gera-planilha.py.
#
# Autores: Gustavo Gabriel Ripka <GRR20203935>, Arthur Barreto Godoi <GRR20224377>
#
# Uso: python3 scripts/gera-planilha-ods.py
# (gera um .fods intermediario e converte para .ods com o LibreOffice)

import os
import subprocess
import sys
import tempfile

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAN = os.path.join(BASE, "planilha")

CAB = """<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
 xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
 xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
 xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
 office:mimetype="application/vnd.oasis.opendocument.spreadsheet"
 office:version="1.2">
 <office:body>
  <office:spreadsheet>
"""

RODAPE = """  </office:spreadsheet>
 </office:body>
</office:document>
"""


def celula(valor):
    try:
        num = float(valor)
    except ValueError:
        return (f'<table:table-cell office:value-type="string">'
                f'<text:p>{valor}</text:p></table:table-cell>')
    return (f'<table:table-cell office:value-type="float" '
            f'office:value="{num}"><text:p>{valor}</text:p></table:table-cell>')


def folha(nome, linhas):
    out = [f'   <table:table table:name="{nome}">']
    for linha in linhas:
        out.append("    <table:table-row>")
        for valor in linha:
            out.append("     " + celula(valor))
        out.append("    </table:table-row>")
    out.append("   </table:table>")
    return "\n".join(out)


def le_csv(nome, sep=";"):
    with open(os.path.join(PLAN, nome)) as f:
        return [l.rstrip("\n").split(sep) for l in f if l.strip()]


def main():
    resumo = le_csv("planilha-resultados.csv")
    det = le_csv("execucoes-detalhado.csv", sep=",")

    exp_a = [["Versão A (simples) — 2304x960x960 doubles"],
             ["nodos", "tempo total (s)", "computação (s)",
              "comunicação (s)", "GFLOPS", "aceleração vs 1 nodo"]]
    exp_b = [["Versão B (blocada, w=192) — 2304x960x960 doubles"],
             ["nodos", "tempo total (s)", "computação (s)",
              "comunicação (s)", "GFLOPS", "aceleração vs 1 nodo"]]
    comp = [["Comparação A × B — médias de 10 execuções"],
            ["nodos", "tempo A (s)", "tempo B (s)",
             "aceleração B/A", "GFLOPS B", "GFLOPS A"]]

    for linha in resumo[1:]:
        if linha[0] == "A":
            exp_a.append([linha[1], linha[2], linha[3], linha[4],
                          linha[5], linha[6]])
        elif linha[0] == "B":
            exp_b.append([linha[1], linha[2], linha[3], linha[4],
                          linha[5], linha[6]])
    for linha in resumo[1:]:
        if linha[0] == "B/A":
            nodos = linha[1]
            gf_a = gf_b = "-"
            for l in resumo[1:]:
                if l[0] == "A" and l[1] == nodos:
                    gf_a = l[5]
                if l[0] == "B" and l[1] == nodos:
                    gf_b = l[5]
            comp.append([nodos, linha[3], linha[2], linha[4], gf_b, gf_a])

    det_folha = [["Execuções individuais (10 por configuração)"]] + det

    fods = (CAB
            + folha("Exp A", exp_a)
            + folha("Exp B", exp_b)
            + folha("Comparacao A-B", comp)
            + folha("Execucoes", det_folha)
            + RODAPE)

    with tempfile.TemporaryDirectory() as tmp:
        caminho = os.path.join(tmp, "planilha-resultados.fods")
        with open(caminho, "w") as f:
            f.write(fods)
        r = subprocess.run(["soffice", "--headless", "--convert-to", "ods",
                            "--outdir", PLAN, caminho],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"conversao falhou: {r.stderr}")

    print("ok:", os.path.join(PLAN, "planilha-resultados.ods"))


if __name__ == "__main__":
    main()
