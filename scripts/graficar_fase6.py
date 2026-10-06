#!/usr/bin/env python3
"""Fase 6: tablas y gráficas adicionales sobre el benchmark.

Lee results/tiempos.csv y results/tiempos_mpi_docker.csv. Genera:
  results/resumen_cpu_real.csv       -- (4a) CPU/real del filtrado por versión
  results/resumen_total.csv          -- (4b) tiempo total y speedup total
  results/resumen_amdahl.csv         -- (4c) fracción secuencial medida y speedup máx. (Amdahl)
  results/graficas/05_speedup_vs_pixeles.png
  results/graficas/06_speedup_filtrado_vs_total.png

No interpreta los resultados, solo agrega y grafica (igual criterio que
scripts/graficar.py).
"""
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent))
from graficar import cargar_datos, agregar_por_ejecucion, etiqueta_version, ORDEN_VERSIONES, RAIZ, DIR_GRAFICAS

CSV_MPI_DOCKER = RAIZ / "results" / "tiempos_mpi_docker.csv"
RESULTS_DIR = RAIZ / "results"

IMAGENES_PIXELES = {
    "lena": 512 * 512,
    "fruit": 900 * 450,
    "puj": 1920 * 600,
    "sulfur": 823 * 1000,
    "damma": 1000 * 1278,
}


def cargar_mpi_docker():
    df = pd.read_csv(CSV_MPI_DOCKER)
    num_cols = ["hilos_o_nodos", "repeticion", "ancho", "alto",
                "lectura_real_s", "lectura_cpu_s", "filtrado_real_s", "filtrado_cpu_s",
                "escritura_real_s", "escritura_cpu_s", "comunicacion_real_s", "comunicacion_cpu_s",
                "total_real_s", "total_cpu_s"]
    for c in num_cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")
    grp = ["programa", "hilos_o_nodos", "imagen", "formato", "repeticion"]
    # Solo el rank 0 trae lectura/escritura/total (no nulos); max() con
    # skipna de pandas devuelve ese único valor sin filtrar por rank.
    agg = df.groupby(grp).agg(
        lectura_real_s=("lectura_real_s", "max"),
        lectura_cpu_s=("lectura_cpu_s", "max"),
        filtrado_real_s=("filtrado_real_s", "max"),
        filtrado_cpu_s=("filtrado_cpu_s", "max"),
        escritura_real_s=("escritura_real_s", "max"),
        escritura_cpu_s=("escritura_cpu_s", "max"),
        comunicacion_real_s=("comunicacion_real_s", "max"),
        comunicacion_cpu_s=("comunicacion_cpu_s", "max"),
    ).reset_index()
    return agg


def con_total(ejecuciones):
    e = ejecuciones.copy()
    partes_r = ["lectura_real_s", "filtrado_real_s", "escritura_real_s", "comunicacion_real_s"]
    partes_c = ["lectura_cpu_s", "filtrado_cpu_s", "escritura_cpu_s", "comunicacion_cpu_s"]
    e["total_real_s"] = e[partes_r].fillna(0).sum(axis=1)
    e["total_cpu_s"] = e[partes_c].fillna(0).sum(axis=1)
    return e


def etiqueta(row):
    if row["programa"] == "mpi-docker":
        return f"mpi-docker-{row['hilos_o_nodos']}"
    return etiqueta_version(row["programa"], row["hilos_o_nodos"])


def main():
    df_local = cargar_datos()
    ejec_local = agregar_por_ejecucion(df_local)
    ejec_docker = cargar_mpi_docker()

    ejecuciones = pd.concat([ejec_local, ejec_docker], ignore_index=True)
    ejecuciones = con_total(ejecuciones)
    ejecuciones["version"] = ejecuciones.apply(etiqueta, axis=1)

    # Restringido a damma+sulfur (las únicas imágenes comunes a las 4
    # versiones con todos sus conteos de hilos/nodos) para las tablas que
    # comparan contra el secuencial.
    comunes = ejecuciones[ejecuciones["imagen"].isin(["damma", "sulfur"])].copy()

    base_filtrado = comunes.loc[comunes["programa"] == "secuencial", "filtrado_real_s"].mean()
    base_total = comunes.loc[comunes["programa"] == "secuencial", "total_real_s"].mean()

    resumen = comunes.groupby("version").agg(
        programa=("programa", "first"),
        hilos_o_nodos=("hilos_o_nodos", "first"),
        n=("filtrado_real_s", "count"),
        filtrado_real_media=("filtrado_real_s", "mean"),
        filtrado_real_std=("filtrado_real_s", "std"),
        filtrado_cpu_media=("filtrado_cpu_s", "mean"),
        filtrado_cpu_std=("filtrado_cpu_s", "std"),
        total_real_media=("total_real_s", "mean"),
        total_real_std=("total_real_s", "std"),
    ).reset_index()
    resumen["orden"] = resumen["version"].apply(lambda v: ORDEN_VERSIONES.index(v) if v in ORDEN_VERSIONES else (900 if "mpi-docker" in v else 999))
    resumen = resumen.sort_values("orden").drop(columns="orden").reset_index(drop=True)

    # --- (4a) CPU/real del filtrado ---
    cpu_real = resumen[["version", "programa", "hilos_o_nodos", "n",
                         "filtrado_real_media", "filtrado_cpu_media"]].copy()
    cpu_real["razon_cpu_real"] = cpu_real["filtrado_cpu_media"] / cpu_real["filtrado_real_media"]
    cpu_real.to_csv(RESULTS_DIR / "resumen_cpu_real.csv", index=False)
    print("=== (4a) CPU/real del filtrado ===")
    print(cpu_real.to_string(index=False))

    # --- (4b) tiempo total y speedup total ---
    total_tbl = resumen[["version", "programa", "hilos_o_nodos", "n",
                          "total_real_media", "total_real_std"]].copy()
    total_tbl["speedup_total"] = base_total / total_tbl["total_real_media"]
    total_tbl["speedup_filtrado"] = base_filtrado / resumen["filtrado_real_media"]
    total_tbl.to_csv(RESULTS_DIR / "resumen_total.csv", index=False)
    print("\n=== (4b) tiempo total y speedup total (y speedup de filtrado, para comparar) ===")
    print(total_tbl.to_string(index=False))

    # --- (4c) fracción secuencial medida (Ley de Amdahl) ---
    # Se estima la fracción no paralelizable 'f' a partir del speedup medido
    # con el mayor paralelismo probado de cada versión, despejando de
    # Amdahl: speedup = 1 / (f + (1-f)/n)  =>  f = (1/speedup - 1/n) / (1 - 1/n)
    amdahl_filas = []
    for prog, n_max in [("pthreads", 4), ("openmp", 12), ("mpi", 4), ("mpi-docker", 4)]:
        fila = total_tbl[(total_tbl["programa"] == prog) & (total_tbl["hilos_o_nodos"] == n_max)]
        if fila.empty:
            continue
        s = fila["speedup_filtrado"].iloc[0]
        n = n_max
        if n > 1 and s > 1:
            f = (1 / s - 1 / n) / (1 - 1 / n)
        else:
            f = np.nan
        f = max(0.0, f) if not np.isnan(f) else f
        speedup_max_teorico = 1 / f if (f and f > 0) else np.inf
        amdahl_filas.append({
            "programa": prog, "n_usado": n, "speedup_medido": s,
            "fraccion_secuencial_f": f, "speedup_maximo_teorico_n_infinito": speedup_max_teorico,
        })
    amdahl = pd.DataFrame(amdahl_filas)
    amdahl.to_csv(RESULTS_DIR / "resumen_amdahl.csv", index=False)
    print("\n=== (4c) Fracción secuencial medida y speedup máximo teórico (Amdahl) ===")
    print(amdahl.to_string(index=False))

    # --- Gráfica: speedup del filtrado vs número de píxeles (tamaño) ---
    tam = ejecuciones[ejecuciones["imagen"].isin(IMAGENES_PIXELES.keys())].copy()
    tam["pixeles"] = tam["imagen"].map(IMAGENES_PIXELES) * tam["formato"].map({"pgm": 1, "ppm": 3})
    # Un punto por versión "de referencia" (máximo paralelismo probado) x imagen
    seq_por_imgfmt = tam[tam["programa"] == "secuencial"].groupby(["imagen", "formato"])["filtrado_real_s"].mean()

    fig, ax = plt.subplots(figsize=(8, 5.5))
    configs = [("pthreads", 4, "pthreads-4", "o"), ("openmp", 12, "openmp-12", "s"),
               ("mpi", 4, "mpi-4 (local)", "^"), ("mpi-docker", 4, "mpi-4 (docker)", "D")]
    for prog, hon, etiq, marker in configs:
        sub = tam[(tam["programa"] == prog) & (tam["hilos_o_nodos"] == hon)]
        if sub.empty:
            continue
        agg = sub.groupby(["imagen", "formato"]).agg(
            filtrado_real_s=("filtrado_real_s", "mean"), pixeles=("pixeles", "first")
        ).reset_index()
        agg["speedup"] = agg.apply(
            lambda r: seq_por_imgfmt.get((r["imagen"], r["formato"]), np.nan) / r["filtrado_real_s"], axis=1
        )
        agg = agg.sort_values("pixeles")
        ax.plot(agg["pixeles"], agg["speedup"], marker=marker, linestyle="-", label=etiq)
    ax.set_xscale("log")
    ax.set_xlabel("Número de píxeles de la imagen (ancho x alto x canales, escala log)")
    ax.set_ylabel("Speedup del filtrado (t_secuencial / t_versión)")
    ax.set_title("Speedup del filtrado vs. tamaño de la imagen\n(lena, fruit, puj, sulfur, damma; PGM y PPM)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(DIR_GRAFICAS / "05_speedup_vs_pixeles.png", dpi=150)
    plt.close(fig)

    # --- Gráfica: speedup del filtrado vs speedup total ---
    fig, ax = plt.subplots(figsize=(7, 6))
    maxv = max(total_tbl["speedup_filtrado"].max(), total_tbl["speedup_total"].max(), 1) * 1.1
    ref = np.linspace(0, maxv, 10)
    ax.plot(ref, ref, "--", color="gray", label="speedup_filtrado = speedup_total")
    colores = {"secuencial": "#888888", "pthreads": "#DD8452", "openmp": "#4C72B0",
               "mpi": "#55A868", "mpi-docker": "#C44E52"}
    for _, r in total_tbl.iterrows():
        ax.scatter(r["speedup_filtrado"], r["speedup_total"],
                    color=colores.get(r["programa"], "black"))
        ax.annotate(r["version"], (r["speedup_filtrado"], r["speedup_total"]), fontsize=7,
                    textcoords="offset points", xytext=(4, 3))
    ax.set_xlabel("Speedup del filtrado")
    ax.set_ylabel("Speedup total (lectura+filtrado+escritura[+comunicación])")
    ax.set_title("Speedup del filtrado vs. speedup total, por versión\n(damma+sulfur, pgm+ppm)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(DIR_GRAFICAS / "06_speedup_filtrado_vs_total.png", dpi=150)
    plt.close(fig)

    print(f"\nGráficas y tablas de la Fase 6 guardadas en {DIR_GRAFICAS} y {RESULTS_DIR}")


if __name__ == "__main__":
    main()
