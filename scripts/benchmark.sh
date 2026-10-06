#!/bin/bash
# Fase 5: corre las 4 versiones del filtrado (secuencial, pthreads, OpenMP
# con varios conteos de hilos, MPI con varios conteos de nodos) sobre las
# imágenes aplicables a cada una, 5 repeticiones por combinación, y
# normaliza todas las salidas en un único results/tiempos.csv.
#
# Uso: scripts/benchmark.sh   (ejecutar desde cualquier lado; se ubica solo
# en la raíz del repo)
set -euo pipefail

cd "$(dirname "$0")/.."

REPETICIONES=5
HILOS_OMP=(1 2 4 12)   # 12 = núcleos lógicos de esta máquina (nproc --all)
NODOS_MPI=(1 2 4)

# Imágenes "aplicables" por versión, siguiendo la misma asignación que
# CLAUDE.md define para cada diseño (Diseño 2: lena/fruit/puj; Diseño 3:
# damma/sulfur; Diseño 4: damma/sulfur). damma y sulfur se agregan también
# a la lista de la versión secuencial porque pthreads/OpenMP/MPI las
# necesitan como línea base para calcular speedup y eficiencia.
IMAGENES_SECUENCIAL="lena fruit puj damma sulfur"
IMAGENES_PARALELO="damma sulfur"

RESULTS_DIR="results"
CSV="$RESULTS_DIR/tiempos.csv"
TMP_SALIDAS=$(mktemp -d)
trap 'rm -rf "$TMP_SALIDAS"' EXIT

mkdir -p "$RESULTS_DIR"

echo "== Estado del sistema antes de medir =="
uptime
echo "Núcleos lógicos: $(nproc --all)"
echo "NOTA: no se cerraron procesos del usuario automáticamente (decisión"
echo "tomada junto con el usuario); la carga de fondo queda documentada"
echo "tal cual en docs/reportes/FASE_5.md."
echo

# Detiene (si estuviera arriba) el clúster Docker de la Fase 4; no afecta
# otros contenedores del usuario (proyecto con nombre propio "mp2mpi").
docker compose -p mp2mpi down >/dev/null 2>&1 || true

echo "== Compilando con -O2 =="
source /etc/profile.d/modules.sh 2>/dev/null || true
module load mpi/openmpi-x86_64 2>/dev/null || true
make clean
make OPT=-O2 all mpi_filterer

echo
echo "programa,hilos_o_nodos,imagen,formato,repeticion,rank,filtro,ancho,alto,lectura_real_s,lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,escritura_real_s,escritura_cpu_s,comunicacion_real_s,comunicacion_cpu_s,total_real_s,total_cpu_s" > "$CSV"

# Convierte la salida CSV de filterer/th_filterer/omp_filterer (esquema:
# ejecutor,entrada,salida,filtro,ancho,alto,lectura_real_s,lectura_cpu_s,
# filtrado_real_s,filtrado_cpu_s,escritura_real_s,escritura_cpu_s,
# total_real_s,total_cpu_s) al esquema unificado de results/tiempos.csv.
normalizar_simple() {
  awk -F',' -v prog="$1" -v hon="$2" -v img="$3" -v fmt="$4" -v rep="$5" '
    NR==1 { next }
    {
      printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n", \
        prog,hon,img,fmt,rep,"",$4,$5,$6,$7,$8,$9,$10,$11,$12,"","",$13,$14
    }'
}

# Convierte la salida CSV de mpi_filterer (esquema: rank,size,entrada,
# ancho,alto,filas_propias,filtrado_real_s,filtrado_cpu_s,
# comunicacion_real_s,comunicacion_cpu_s) al esquema unificado.
normalizar_mpi() {
  awk -F',' -v img="$1" -v fmt="$2" -v rep="$3" '
    NR==1 { next }
    {
      printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n", \
        "mpi",$2,img,fmt,rep,$1,"todos",$4,$5,"","",$7,$8,"","",$9,$10,"",""
    }'
}

echo "== Secuencial (filterer) =="
for img in $IMAGENES_SECUENCIAL; do
  for ext in pgm ppm; do
    f="images/${img}.${ext}"
    [ -f "$f" ] || continue
    for rep in $(seq 1 "$REPETICIONES"); do
      echo "  filterer $img.$ext rep=$rep"
      ./filterer "$f" "$TMP_SALIDAS/${img}_seq.${ext}" | normalizar_simple "secuencial" 1 "$img" "$ext" "$rep" >> "$CSV"
    done
  done
done

echo "== pthreads (th_filterer, 4 cuadrantes) =="
for img in $IMAGENES_PARALELO; do
  for ext in pgm ppm; do
    f="images/${img}.${ext}"
    for rep in $(seq 1 "$REPETICIONES"); do
      echo "  th_filterer $img.$ext rep=$rep"
      ./th_filterer "$f" "$TMP_SALIDAS/${img}_th.${ext}" | normalizar_simple "pthreads" 4 "$img" "$ext" "$rep" >> "$CSV"
    done
  done
done

echo "== OpenMP (omp_filterer) =="
for img in $IMAGENES_PARALELO; do
  for ext in pgm ppm; do
    f="images/${img}.${ext}"
    for hilos in "${HILOS_OMP[@]}"; do
      for rep in $(seq 1 "$REPETICIONES"); do
        echo "  omp_filterer $img.$ext hilos=$hilos rep=$rep"
        ./omp_filterer "$f" "$TMP_SALIDAS/${img}_omp.${ext}" --hilos "$hilos" | normalizar_simple "openmp" "$hilos" "$img" "$ext" "$rep" >> "$CSV"
      done
    done
  done
done

echo "== MPI (mpi_filterer, local en esta máquina) =="
for img in $IMAGENES_PARALELO; do
  for ext in pgm ppm; do
    f="images/${img}.${ext}"
    for np in "${NODOS_MPI[@]}"; do
      for rep in $(seq 1 "$REPETICIONES"); do
        echo "  mpi_filterer $img.$ext np=$np rep=$rep"
        mpirun --bind-to none -np "$np" ./mpi_filterer "$f" "$TMP_SALIDAS/${img}_mpi.${ext}" | normalizar_mpi "$img" "$ext" "$rep" >> "$CSV"
      done
    done
  done
done

echo
echo "Listo: $CSV ($(wc -l < "$CSV") líneas, incluyendo encabezado)"
