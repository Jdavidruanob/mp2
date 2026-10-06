#!/bin/bash
# Compila mpi_filterer y lo ejecuta con mpirun, dentro de un contenedor del
# clúster de docker-compose.yml (se asume /proyecto montado ahí, que es el
# bind mount del repo completo).
#
# Uso:
#   ./run_mpi.sh local   <np> <entrada> <salida>   # un solo contenedor, sin SSH ni hostfile
#   ./run_mpi.sh cluster <np> <entrada> <salida>   # entre contenedores, usando hostfile + SSH
#
# Ejemplos (desde el host, vía docker compose exec):
#   docker compose exec maestro /proyecto/run_mpi.sh local   4 images/damma.pgm output/damma.pgm
#   docker compose exec maestro /proyecto/run_mpi.sh cluster 4 images/damma.pgm output/damma.pgm
set -euo pipefail

if [ "$#" -ne 4 ]; then
  echo "Uso: $0 <local|cluster> <np> <entrada> <salida>" >&2
  exit 1
fi

MODO="$1"
NP="$2"
ENTRADA="$3"
SALIDA="$4"

cd /proyecto
make mpi_filterer

# --bind-to none: los contenedores no tienen un cpuset propio (comparten
# la topología completa del host), así que el binding por defecto de
# OpenMPI (atar cada rank a "core 0" de su propia vista de la topología)
# hace que procesos de distintos contenedores terminen atados al MISMO
# núcleo físico del host y se bloqueen entre sí. Ver docs/reportes/FASE_4.md
# ("Problemas encontrados") para la evidencia con --report-bindings.
case "$MODO" in
  local)
    mpirun --allow-run-as-root --bind-to none -np "$NP" ./mpi_filterer "$ENTRADA" "$SALIDA"
    ;;
  cluster)
    mpirun --allow-run-as-root --bind-to none --hostfile /proyecto/hostfile -np "$NP" ./mpi_filterer "$ENTRADA" "$SALIDA"
    ;;
  *)
    echo "Modo desconocido: '$MODO' (use 'local' o 'cluster')" >&2
    exit 1
    ;;
esac
