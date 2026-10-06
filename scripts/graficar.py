#!/usr/bin/env python3
"""Fase 5: genera las gráficas y las tablas resumen a partir de
results/tiempos.csv (producido por scripts/benchmark.sh).

Salidas:
  results/graficas/01_filtrado_por_version.png
  results/graficas/02_speedup.png
  results/graficas/03_eficiencia.png
  results/graficas/04_desglose_io.png
  results/resumen.csv   (tabla promedio/desviación estándar, por versión)

No interpreta los resultados: solo agrega (promedio, desviación estándar)
y grafica. La metodología de agregación (por qué se suma o se toma el
máximo en cada caso) está documentada en los comentarios y en
docs/reportes/FASE_5.md.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
CSV_ENTRADA = RAIZ / "results" / "tiempos.csv"
DIR_GRAFICAS = RAIZ / "results" / "graficas"
CSV_RESUMEN = RAIZ / "results" / "resumen.csv"

DIR_GRAFICAS.mkdir(parents=True, exist_ok=True)

NUM_COLS = [
    "ancho", "alto",
    "lectura_real_s", "lectura_cpu_s",
    "filtrado_real_s", "filtrado_cpu_s",
    "escritura_real_s", "escritura_cpu_s",
    "comunicacion_real_s", "comunicacion_cpu_s",
    "total_real_s", "total_cpu_s",
]


def cargar_datos():
    df = pd.read_csv(CSV_ENTRADA)
    for c in NUM_COLS:
        df[c] = pd.to_numeric(df[c], errors="coerce")
    return df


def agregar_por_ejecucion(df):
    """Reduce el CSV "largo" (una fila por filtro, o por rank en MPI) a una
    fila por ejecución real: (programa, hilos_o_nodos, imagen, formato,
    repeticion).

    - Secuencial/pthreads/OpenMP: cada ejecución aplica 3 filtros uno
      después de otro (ver EjecucionCLI.cpp); lectura_real_s/lectura_cpu_s
      son el MISMO valor repetido en las 3 filas (se lee una sola vez), así
      que se toma el primero, no se suma. filtrado_real_s y escritura_real_s
      sí son distintos por filtro (se miden por separado) y SÍ se suman
      para obtener el tiempo total de la ejecución completa.
    - MPI: cada ejecución reporta una fila por rank, con filtrado_real_s ya
      sumado internamente sobre los 3 filtros (ver mpi_filterer.cpp). Los
      ranks corren en paralelo, así que el tiempo de pared de la ejecución
      es el máximo entre ranks, no la suma. mpi_filterer no mide
      lectura/escritura por separado (solo filtrado y comunicación, que es
      lo que pedía la Fase 4), así que esas columnas quedan NaN para MPI.
    """
    grp = ["programa", "hilos_o_nodos", "imagen", "formato", "repeticion"]

    es_mpi = df["programa"] == "mpi"

    no_mpi = df[~es_mpi]
    agg_no_mpi = no_mpi.groupby(grp).agg(
        lectura_real_s=("lectura_real_s", "first"),
        lectura_cpu_s=("lectura_cpu_s", "first"),
        filtrado_real_s=("filtrado_real_s", "sum"),
        filtrado_cpu_s=("filtrado_cpu_s", "sum"),
        escritura_real_s=("escritura_real_s", "sum"),
        escritura_cpu_s=("escritura_cpu_s", "sum"),
    ).reset_index()
    agg_no_mpi["comunicacion_real_s"] = np.nan
    agg_no_mpi["comunicacion_cpu_s"] = np.nan

    mpi_df = df[es_mpi]
    # Desde la Fase 6, mpi_filterer también mide lectura/escritura/total,
    # pero SOLO el rank 0 las reporta (los demás ranks no leen ni escriben
    # archivo); al venir NaN en el resto, max() con skipna (por defecto en
    # pandas) devuelve directamente el valor del rank 0 sin necesidad de
    # filtrar por rank explícitamente.
    agg_mpi = mpi_df.groupby(grp).agg(
        filtrado_real_s=("filtrado_real_s", "max"),
        filtrado_cpu_s=("filtrado_cpu_s", "max"),
        comunicacion_real_s=("comunicacion_real_s", "max"),
        comunicacion_cpu_s=("comunicacion_cpu_s", "max"),
        lectura_real_s=("lectura_real_s", "max"),
        lectura_cpu_s=("lectura_cpu_s", "max"),
        escritura_real_s=("escritura_real_s", "max"),
        escritura_cpu_s=("escritura_cpu_s", "max"),
    ).reset_index()

    ejecuciones = pd.concat([agg_no_mpi, agg_mpi], ignore_index=True)
    return ejecuciones


def etiqueta_version(programa, hilos_o_nodos):
    if programa == "secuencial":
        return "secuencial"
    if programa == "pthreads":
        return f"pthreads-{hilos_o_nodos}"
    if programa == "openmp":
        return f"openmp-{hilos_o_nodos}"
    if programa == "mpi":
        return f"mpi-{hilos_o_nodos}"
    return f"{programa}-{hilos_o_nodos}"


ORDEN_VERSIONES = [
    "secuencial", "pthreads-4",
    "openmp-1", "openmp-2", "openmp-4", "openmp-12",
    "mpi-1", "mpi-2", "mpi-4",
]


def construir_resumen(ejecuciones):
    """Tabla promedio/desviación estándar por versión, restringida a las
    imágenes comunes a las 4 versiones (damma y sulfur, pgm y ppm) para que
    la comparación entre versiones no esté sesgada por imágenes de distinto
    tamaño."""
    comunes = ejecuciones[ejecuciones["imagen"].isin(["damma", "sulfur"])].copy()
    comunes["version"] = comunes.apply(
        lambda r: etiqueta_version(r["programa"], r["hilos_o_nodos"]), axis=1
    )

    resumen = comunes.groupby("version").agg(
        programa=("programa", "first"),
        hilos_o_nodos=("hilos_o_nodos", "first"),
        n_muestras=("filtrado_real_s", "count"),
        filtrado_real_s_media=("filtrado_real_s", "mean"),
        filtrado_real_s_std=("filtrado_real_s", "std"),
        filtrado_cpu_s_media=("filtrado_cpu_s", "mean"),
        filtrado_cpu_s_std=("filtrado_cpu_s", "std"),
        comunicacion_real_s_media=("comunicacion_real_s", "mean"),
        comunicacion_real_s_std=("comunicacion_real_s", "std"),
    ).reset_index()

    base = resumen.loc[resumen["version"] == "secuencial", "filtrado_real_s_media"].iloc[0]
    resumen["speedup"] = base / resumen["filtrado_real_s_media"]
    resumen["eficiencia"] = resumen["speedup"] / resumen["hilos_o_nodos"]

    resumen["orden"] = resumen["version"].apply(
        lambda v: ORDEN_VERSIONES.index(v) if v in ORDEN_VERSIONES else 999
    )
    resumen = resumen.sort_values("orden").drop(columns="orden").reset_index(drop=True)
    return resumen


def graficar_filtrado_por_version(resumen):
    fig, ax = plt.subplots(figsize=(10, 5))
    x = np.arange(len(resumen))
    ax.bar(x, resumen["filtrado_real_s_media"], yerr=resumen["filtrado_real_s_std"],
           capsize=4, color="#4C72B0")
    ax.set_xticks(x)
    ax.set_xticklabels(resumen["version"], rotation=45, ha="right")
    ax.set_ylabel("Tiempo de filtrado real (s)")
    ax.set_title("Tiempo de filtrado por versión\n(promedio ± desv. estándar, damma+sulfur, pgm+ppm, 5 repeticiones)")
    fig.tight_layout()
    fig.savefig(DIR_GRAFICAS / "01_filtrado_por_version.png", dpi=150)
    plt.close(fig)


def graficar_speedup_eficiencia(resumen):
    series = {
        "pthreads": resumen[resumen["programa"] == "pthreads"],
        "openmp": resumen[resumen["programa"] == "openmp"].sort_values("hilos_o_nodos"),
        "mpi": resumen[resumen["programa"] == "mpi"].sort_values("hilos_o_nodos"),
    }
    colores = {"pthreads": "#DD8452", "openmp": "#4C72B0", "mpi": "#55A868"}

    max_h = max(resumen["hilos_o_nodos"].max(), 1)

    # --- Speedup ---
    fig, ax = plt.subplots(figsize=(7, 5.5))
    ref = np.arange(1, max_h + 1)
    ax.plot(ref, ref, "--", color="gray", label="speedup ideal")
    for nombre, datos in series.items():
        if datos.empty:
            continue
        marker = "o" if len(datos) > 1 else "x"
        ax.plot(datos["hilos_o_nodos"], datos["speedup"], marker=marker,
                color=colores[nombre], label=nombre)
    ax.set_xlabel("Hilos (pthreads/OpenMP) o nodos (MPI)")
    ax.set_ylabel("Speedup (t_secuencial / t_versión)")
    ax.set_title("Speedup del filtrado por número de hilos/nodos\n(damma+sulfur, pgm+ppm)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(DIR_GRAFICAS / "02_speedup.png", dpi=150)
    plt.close(fig)

    # --- Eficiencia ---
    fig, ax = plt.subplots(figsize=(7, 5.5))
    ax.axhline(1.0, linestyle="--", color="gray", label="eficiencia ideal (1.0)")
    for nombre, datos in series.items():
        if datos.empty:
            continue
        marker = "o" if len(datos) > 1 else "x"
        ax.plot(datos["hilos_o_nodos"], datos["eficiencia"], marker=marker,
                color=colores[nombre], label=nombre)
    ax.set_xlabel("Hilos (pthreads/OpenMP) o nodos (MPI)")
    ax.set_ylabel("Eficiencia (speedup / hilos_o_nodos)")
    ax.set_title("Eficiencia del filtrado por número de hilos/nodos\n(damma+sulfur, pgm+ppm)")
    ax.set_ylim(0, 1.15)
    ax.legend()
    fig.tight_layout()
    fig.savefig(DIR_GRAFICAS / "03_eficiencia.png", dpi=150)
    plt.close(fig)


def graficar_desglose_io(ejecuciones):
    """Barras apiladas lectura/filtrado/escritura. Solo secuencial,
    pthreads y OpenMP: mpi_filterer no mide lectura/escritura por separado
    (ver agregar_por_ejecucion). Usa cada programa con TODAS sus imágenes
    aplicables (no solo damma/sulfur) para mostrar el peso de la E/S en el
    conjunto de pruebas real de cada versión."""
    datos = ejecuciones[ejecuciones["programa"] != "mpi"].copy()
    datos["version"] = datos.apply(
        lambda r: etiqueta_version(r["programa"], r["hilos_o_nodos"]), axis=1
    )
    resumen = datos.groupby("version").agg(
        lectura=("lectura_real_s", "mean"),
        filtrado=("filtrado_real_s", "mean"),
        escritura=("escritura_real_s", "mean"),
        orden=("hilos_o_nodos", "first"),
        programa=("programa", "first"),
    ).reset_index()
    resumen["orden2"] = resumen["version"].apply(
        lambda v: ORDEN_VERSIONES.index(v) if v in ORDEN_VERSIONES else 999
    )
    resumen = resumen.sort_values("orden2")

    fig, ax = plt.subplots(figsize=(10, 5.5))
    x = np.arange(len(resumen))
    ax.bar(x, resumen["lectura"], label="lectura", color="#4C72B0")
    ax.bar(x, resumen["filtrado"], bottom=resumen["lectura"], label="filtrado", color="#DD8452")
    ax.bar(x, resumen["escritura"], bottom=resumen["lectura"] + resumen["filtrado"],
           label="escritura", color="#55A868")
    ax.set_xticks(x)
    ax.set_xticklabels(resumen["version"], rotation=45, ha="right")
    ax.set_ylabel("Tiempo real (s)")
    ax.set_title("Desglose lectura / filtrado / escritura por versión\n(promedio sobre todas sus imágenes aplicables, 5 repeticiones)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(DIR_GRAFICAS / "04_desglose_io.png", dpi=150)
    plt.close(fig)


def construir_resumen_detallado(ejecuciones):
    """Promedio/desviación estándar por (versión, imagen, formato), sobre
    las 5 repeticiones únicamente -- a diferencia de construir_resumen(),
    aquí el std no mezcla la variación entre imágenes distintas con el
    ruido entre repeticiones."""
    datos = ejecuciones.copy()
    datos["version"] = datos.apply(
        lambda r: etiqueta_version(r["programa"], r["hilos_o_nodos"]), axis=1
    )
    detalle = datos.groupby(["version", "imagen", "formato"]).agg(
        programa=("programa", "first"),
        hilos_o_nodos=("hilos_o_nodos", "first"),
        n_muestras=("filtrado_real_s", "count"),
        filtrado_real_s_media=("filtrado_real_s", "mean"),
        filtrado_real_s_std=("filtrado_real_s", "std"),
        lectura_real_s_media=("lectura_real_s", "mean"),
        escritura_real_s_media=("escritura_real_s", "mean"),
    ).reset_index()
    detalle["orden"] = detalle["version"].apply(
        lambda v: ORDEN_VERSIONES.index(v) if v in ORDEN_VERSIONES else 999
    )
    detalle = detalle.sort_values(["orden", "imagen", "formato"]).drop(columns="orden").reset_index(drop=True)
    return detalle


def main():
    df = cargar_datos()
    ejecuciones = agregar_por_ejecucion(df)

    resumen = construir_resumen(ejecuciones)
    resumen.to_csv(CSV_RESUMEN, index=False)
    print(f"Resumen guardado en {CSV_RESUMEN}")
    print(resumen.to_string(index=False))

    detalle = construir_resumen_detallado(ejecuciones)
    ruta_detalle = RAIZ / "results" / "resumen_detallado.csv"
    detalle.to_csv(ruta_detalle, index=False)
    print(f"\nResumen detallado guardado en {ruta_detalle}")
    print(detalle.to_string(index=False))

    graficar_filtrado_por_version(resumen)
    graficar_speedup_eficiencia(resumen)
    graficar_desglose_io(ejecuciones)
    print(f"Gráficas guardadas en {DIR_GRAFICAS}")


if __name__ == "__main__":
    main()
