#!/bin/bash
# Fase 6, punto 2: corre mpi_filterer DENTRO del clúster Docker (maestro +
# 3 trabajadores, vía hostfile + SSH), con 1, 2 y 4 nodos, sobre damma y
# sulfur (PGM y PPM), 5 repeticiones por combinación. Pensado para
# ejecutarse DENTRO de un contenedor del clúster (p. ej.
# "docker compose exec maestro /proyecto/scripts/benchmark_mpi_docker.sh"),
# ya que necesita el hostfile + SSH hacia los demás nodos.
#
# Escribe en results/tiempos_mpi_docker.csv con el mismo esquema unificado
# que results/tiempos.csv (programa="mpi-docker", para poder comparar
# directamente contra las filas programa="mpi" -corridas localmente, sin
# Docker- de results/tiempos.csv).
set -euo pipefail

cd /proyecto

REPETICIONES=5
NODOS_MPI=(1 2 4)
IMAGENES="damma sulfur"

RESULTS_DIR="results"
CSV="$RESULTS_DIR/tiempos_mpi_docker.csv"
mkdir -p "$RESULTS_DIR"

echo "== Compilando con -O2 =="
make clean
make OPT=-O2 all mpi_filterer

echo "programa,hilos_o_nodos,imagen,formato,repeticion,rank,filtro,ancho,alto,lectura_real_s,lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,escritura_real_s,escritura_cpu_s,comunicacion_real_s,comunicacion_cpu_s,total_real_s,total_cpu_s" > "$CSV"

# Esquema crudo de mpi_filterer: rank,size,entrada,ancho,alto,
# filas_propias,lectura_real_s,lectura_cpu_s,filtrado_real_s,
# filtrado_cpu_s,comunicacion_real_s,comunicacion_cpu_s,escritura_real_s,
# escritura_cpu_s,total_real_s,total_cpu_s
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
    for np in "${NODOS_MPI[@]}"; do
      for rep in $(seq 1 "$REPETICIONES"); do
        echo "  mpi_filterer (docker) $img.$ext np=$np rep=$rep"
        mpirun --allow-run-as-root --bind-to none --hostfile /proyecto/hostfile -np "$np" \
          ./mpi_filterer "$f" "/tmp/mpi_docker_${img}.${ext}" 2>/dev/null \
          | normalizar_mpi_docker "$img" "$ext" "$rep" >> "$CSV"
      done
    done
  done
done

echo
echo "Listo: $CSV ($(wc -l < "$CSV") líneas, incluyendo encabezado)"
