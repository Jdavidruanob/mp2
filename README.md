# mp2 — Procesamiento de imágenes con programación paralela

Micro-Proyecto 2 del curso Programación Paralela (300CIP013), Pontificia
Universidad Javeriana Cali, 2026-II. Cuatro diseños incrementales de un
filtrador de imágenes PGM/PPM: secuencial, memoria compartida (pthreads y
OpenMP) y memoria distribuida (MPI + Docker). La consigna completa está en
`docs/consigna.pdf`.

## Ramas

`main` es la rama final (y la rama por defecto del repositorio): tiene
todo el trabajo integrado — los 5 ejecutables, la infraestructura Docker,
los scripts de benchmark y los 7 reportes de fase. Las ramas `diseno-1` a
`diseno-4` se conservan **tal como quedaron al cierre de cada fase**
(la consigna exige una rama por diseño); no se vuelven a tocar después de
fusionarse hacia adelante.

| Rama | Contiene | Ejecutable(s) nuevo(s) |
|---|---|---|
| `main` | **Todo integrado** (resultado final) | `processor`, `filterer`, `th_filterer`, `omp_filterer`, `mpi_filterer` |
| `diseno-1` | Lectura/escritura PGM/PPM orientada a objetos | `processor` |
| `diseno-2` | Filtros (blur, laplace, sharpen, sobel) secuenciales + medición de tiempos | `filterer` |
| `diseno-3` | Paralelismo de memoria compartida: 4 cuadrantes (pthreads) y filas (OpenMP) | `th_filterer`, `omp_filterer` |
| `diseno-4` | Paralelismo de memoria distribuida (MPI) + clúster Docker de 4 nodos | `mpi_filterer` |
| `resultados` | Benchmark de las 4 versiones, gráficas y reportes de resultados (fusionada en `main`) | — (scripts/) |

Cada reporte de fase (`docs/reportes/FASE_<n>.md`) documenta las
decisiones de diseño, los errores encontrados y las pruebas de esa rama en
detalle. Este README es solo la guía rápida de compilación/ejecución.

## Arquitectura (resumen)

```
include/ + src/
  Imagen (abstracta) -> PGMImagen (1 canal) / PPMImagen (3 canales)
  ImagenIO            -> lee/escribe, detecta P2/P3, ignora comentarios '#'
  Filtro (abstracta)  -> FiltroBlur / FiltroLaplace / FiltroSharpen / FiltroSobel
  FiltroFactory       -> crea un Filtro* por nombre
  Temporizador         -> tiempo real (steady_clock) y de CPU (clock())
  EjecutorFiltro (abstracta) -> EjecutorSecuencial / EjecutorPthreads / EjecutorOpenMP
  EjecucionCLI         -> CLI + medición + CSV, compartido por filterer/th_filterer/omp_filterer
  ParticionMPI          -> reparto determinista de filas entre ranks MPI (diseno-4)
```

Todos los filtros y todas las estrategias de paralelización (pthreads,
OpenMP, MPI) reutilizan el mismo `Filtro::aplicar(entrada, salida,
y0,y1,x0,x1)`; ninguna reimplementa la convolución. El diseño completo y
su justificación están en `docs/reportes/FASE_2.md` y `FASE_3.md`.

## Requisitos

- g++ con soporte C++17, make.
- Para OpenMP: nada adicional (`libgomp` viene con `gcc-c++`).
- Para pthreads: nada adicional (`-pthread`).
- Para MPI: OpenMPI (`mpic++`, `mpirun`). En Fedora, tras instalar
  `openmpi`+`openmpi-devel`+`environment-modules`, hay que cargar el
  módulo en cada sesión de shell:
  ```bash
  source /etc/profile.d/modules.sh
  module load mpi/openmpi-x86_64
  ```
- Para el clúster Docker (diseno-4): Docker + Docker Compose (el usuario
  debe pertenecer al grupo `docker` o usar `sudo`).
- Para las gráficas (rama `resultados`): Python 3 con matplotlib/pandas/
  numpy (ver sección "Resultados y gráficas" más abajo).

Ver `docs/reportes/FASE_0.md` para el detalle completo de verificación del
entorno.

## Cómo compilar

Desde la raíz del repo, en la rama que corresponda:

```bash
make clean && make           # processor, filterer, th_filterer, omp_filterer
make mpi_filterer             # aparte: necesita mpic++ en el PATH
```

`mpi_filterer` queda fuera de `make`/`make all` a propósito (no todos los
entornos tienen `mpic++` en el `PATH` por defecto); se compila con su
propio target.

Para medir tiempos reales (no para desarrollo/depuración), compilar con
optimización:

```bash
make clean && make OPT=-O2 all mpi_filterer
```

Todo compila con `-Wall -Wextra` sin warnings.

## Cómo ejecutar cada diseño

### Diseño 1 — `processor` (lee y reescribe)

```bash
./processor images/lena.pgm salida.pgm
./processor - salida.pgm < images/lena.pgm   # lectura desde stdin
```

### Diseño 2 — `filterer` (secuencial)

```bash
./filterer images/fruit.ppm salida.ppm --f blur       # un filtro: blur|laplace|sharpen|sobel
./filterer images/fruit.ppm salida.ppm                 # sin --f: blur+laplace+sharpen -> salida_blur/_laplace/_sharpen
```
Imprime una línea CSV por filtro aplicado (tiempos de lectura/filtrado/
escritura, real y CPU).

### Diseño 3 — `th_filterer` (pthreads, 4 cuadrantes) y `omp_filterer` (OpenMP)

```bash
./th_filterer images/damma.pgm salida.pgm --f blur
./omp_filterer images/damma.pgm salida.pgm --f blur --hilos 4   # --hilos opcional; si se omite, usa OMP_NUM_THREADS o el valor por defecto
OMP_NUM_THREADS=8 ./omp_filterer images/damma.pgm salida.pgm
```
Misma sintaxis que `filterer` (con o sin `--f`); mismo formato de CSV.

### Diseño 4 — `mpi_filterer` (MPI)

```bash
mpirun -np 4 ./mpi_filterer images/damma.pgm salida.pgm
```
Siempre aplica los tres filtros obligatorios (no tiene `--f`). Cada rank
imprime su propia línea CSV (filtrado y comunicación, real y CPU).

#### Clúster Docker (1 maestro + 3 trabajadores)

```bash
docker compose build
docker compose up -d
docker compose exec maestro /proyecto/run_mpi.sh local   4 images/damma.pgm output/damma.pgm   # un solo contenedor
docker compose exec maestro /proyecto/run_mpi.sh cluster 4 images/damma.pgm output/damma.pgm   # entre los 4 contenedores (SSH + hostfile)
docker compose down
```
Detalle completo (Dockerfile, red interna, SSH sin contraseña, problemas
encontrados) en `docs/reportes/FASE_4.md`.

## Verificación

Todas las versiones deben producir **salidas idénticas** a `filterer`
(secuencial) para la misma entrada/filtro — se verifica con `cmp`, ver los
reportes de fase correspondientes (Fase 3: pthreads/OpenMP; Fase 4: MPI,
local y entre contenedores).

## Resultados y gráficas

```bash
python3 -m venv .venv
.venv/bin/pip install matplotlib pandas numpy

scripts/benchmark.sh                     # secuencial/pthreads/OpenMP/MPI local, 5 repeticiones -> results/tiempos.csv
docker compose exec maestro /proyecto/scripts/benchmark_mpi_docker.sh   # MPI dentro del clúster Docker -> results/tiempos_mpi_docker.csv

.venv/bin/python scripts/graficar.py        # gráficas 01-04 y results/resumen*.csv
.venv/bin/python scripts/graficar_fase6.py  # gráficas 05-07 (tamaño, Amdahl) y tablas adicionales
```
Tablas de tiempos, speedup/eficiencia, Ley de Amdahl (kernel y programa
completo) y especificaciones de la máquina de prueba en
`docs/reportes/FASE_5.md` y `docs/reportes/FASE_6.md`.

## Imágenes de prueba (`images/`)

lena (512x512 pgm, 128x128 ppm), fruit (900x450), puj (1920x600), sulfur
(823x1000), damma (1000x1278), feep (24x7, prueba de comentarios/formato).
Asignación por diseño: diseño 2 → lena/fruit/puj; diseños 3 y 4 →
damma/sulfur.

## Reportes (`docs/reportes/`)

- [`FASE_0.md`](docs/reportes/FASE_0.md) — verificación del entorno
- [`FASE_1.md`](docs/reportes/FASE_1.md) — diseño 1, errores del código original
- [`FASE_2.md`](docs/reportes/FASE_2.md) — diseño 2, filtros y kernels
- [`FASE_3.md`](docs/reportes/FASE_3.md) — diseño 3, pthreads/OpenMP
- [`FASE_4.md`](docs/reportes/FASE_4.md) — diseño 4, MPI + Docker
- [`FASE_5.md`](docs/reportes/FASE_5.md) — benchmark inicial, gráficas y especificaciones de la máquina
- [`FASE_6.md`](docs/reportes/FASE_6.md) — investigación de la anomalía de escritura, MPI en Docker, impacto del tamaño de imagen, Ley de Amdahl
