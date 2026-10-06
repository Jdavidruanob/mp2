#!/bin/bash
# Fase 6, punto 3: agrega a results/tiempos_mpi_docker.csv las corridas de
# mpi_filterer dentro del clúster Docker para lena/fruit/puj a 4 nodos
# (damma/sulfur con 1/2/4 nodos ya están, ver scripts/benchmark_mpi_docker.sh).
# Pensado para ejecutarse DENTRO de un contenedor del clúster.
set -euo pipefail

cd /proyecto

REPETICIONES=5
IMAGENES="lena fruit puj"

RESULTS_DIR="results"
CSV="$RESULTS_DIR/tiempos_mpi_docker.csv"

normalizar_mpi_docker() {
  awk -F',' -v img="$1" -v fmt="$2" -v rep="$3" '
    NR==1 { next }
    {
      printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n", \
        "mpi-docker",$2,img,fmt,rep,$1,"todos",$4,$5,$7,$8,$9,$10,$13,$14,$11,$12,$15,$16
    }'
}

for img in $IMAGENES; do
  for ext in pgm ppm; do
    f="images/${img}.${ext}"
    [ -f "$f" ] || continue
    for rep in $(seq 1 "$REPETICIONES"); do
      echo "  mpi_filterer (docker) $img.$ext np=4 rep=$rep"
      mpirun --allow-run-as-root --bind-to none --hostfile /proyecto/hostfile -np 4 \
        ./mpi_filterer "$f" "/tmp/mpi_docker_${img}.${ext}" 2>/dev/null \
        | normalizar_mpi_docker "$img" "$ext" "$rep" >> "$CSV"
    done
  done
done

echo "Listo: $CSV ($(wc -l < "$CSV") líneas, incluyendo encabezado)"
