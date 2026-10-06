# Fase 5 — Resultados: benchmark, gráficas y especificaciones de la máquina

Fecha: 2026-10-05
Rama: `resultados` (parte de `diseno-4`)

Este reporte solo presenta los datos recolectados por
`scripts/benchmark.sh` y procesados por `scripts/graficar.py`. No incluye
interpretación de por qué pasa lo que pasa (eso ya se discutió, por
diseño/decisión, en los reportes de las Fases 2 a 4); aquí solo están las
tablas y las gráficas.

## 1. Especificaciones de la máquina

| Componente | Valor |
|---|---|
| CPU | AMD Ryzen 5 5600H with Radeon Graphics |
| Núcleos físicos / lógicos | 6 físicos / 12 lógicos (2 hilos SMT por núcleo), `nproc --all` → 12 |
| Frecuencia | 412.6 MHz – 4280.9 MHz (según `lscpu`, escalado dinámico) |
| RAM | 14 GiB totales (`free -h`) |
| Disco | NVMe SK hynix PC711 512GB (`lsblk`, `ROTA=0` → SSD) |
| SO | Fedora Linux 43 (Workstation Edition) |
| Kernel | 7.2.8-100.fc43.x86_64 |
| Compilador | g++ (GCC) 15.3.1 20260722 (Red Hat 15.3.1-1) |
| MPI | Open MPI 5.0.8 (`mpirun --version`) |

### Estado del sistema al momento de medir

```
$ uptime
22:31:11 up 3:04, 2 users, load average: 1.80, 1.69, 1.47
```

No se cerraron procesos del usuario antes de correr el benchmark (decisión
tomada explícitamente con el usuario: correr con la carga de fondo tal
cual estaba — Discord, Chrome y Spotify activos). El `load average` de
referencia queda documentado aquí porque puede explicar parte de la
dispersión (desviación estándar) de las mediciones.

## 2. Metodología del benchmark (`scripts/benchmark.sh`)

- Compilación: `make clean && make OPT=-O2 all mpi_filterer` (todo el
  proyecto, incluyendo `mpi_filterer`, con optimización `-O2`).
- 5 repeticiones por combinación.
- **Secuencial** (`filterer`): lena, fruit, puj (PGM y PPM — su conjunto de
  prueba "nativo" según `CLAUDE.md`/Fase 2), más damma y sulfur, porque
  pthreads/OpenMP/MPI los necesitan como línea base para calcular speedup
  y eficiencia con imágenes comparables.
- **pthreads** (`th_filterer`, 4 cuadrantes fijos): damma, sulfur (PGM y
  PPM), su conjunto de prueba de la Fase 3.
- **OpenMP** (`omp_filterer`): damma, sulfur (PGM y PPM), con 1, 2, 4 y 12
  hilos (12 = núcleos lógicos de esta máquina).
- **MPI** (`mpi_filterer`): damma, sulfur (PGM y PPM), con 1, 2 y 4
  procesos, `mpirun --bind-to none` (ver `docs/reportes/FASE_4.md`,
  sección de problemas encontrados), corrido localmente en esta máquina
  (sin Docker — la correctitud entre contenedores ya se verificó con `cmp`
  en la Fase 4).
- Cada combinación aplica los tres filtros obligatorios (blur, laplace,
  sharpen; `mpi_filterer` no tiene bandera `--f`, siempre aplica los tres).
- Todo se vuelca, normalizado a un esquema único, en `results/tiempos.csv`
  (591 líneas: 1 encabezado + 590 filas de datos).

### Nota sobre cómo se agregan los datos para las gráficas y tablas de este reporte

`results/tiempos.csv` tiene una fila por filtro (secuencial/pthreads/
OpenMP) o por rank (MPI). `scripts/graficar.py` las agrupa a "una fila por
ejecución" (versión, imagen, formato, repetición) así:

- Secuencial/pthreads/OpenMP: `lectura_real_s` se toma una vez (es el
  mismo valor repetido en las 3 filas del run, se lee una sola vez);
  `filtrado_real_s` y `escritura_real_s` se **suman** entre los 3 filtros
  (se aplican uno después de otro dentro del mismo run).
- MPI: `filtrado_real_s` y `comunicacion_real_s` se toman como el
  **máximo** entre los ranks de esa ejecución (los ranks corren en
  paralelo; el tiempo de pared de la ejecución lo determina el rank más
  lento, no la suma). `mpi_filterer` no mide lectura/escritura por
  separado (solo filtrado y comunicación, que es lo que pedía la Fase 4),
  así que esas columnas quedan vacías para MPI en las tablas/gráficas que
  las usan.

## 3. Tabla resumen — comparación entre versiones

Promedio y desviación estándar de `filtrado_real_s`, calculados sobre
damma+sulfur, PGM+PPM, 5 repeticiones (20 muestras por versión). El
speedup y la eficiencia se calculan contra el promedio de `secuencial` en
este mismo subconjunto de imágenes. Tabla completa en
`results/resumen.csv`.

| versión | n | filtrado real: media (s) | filtrado real: std (s) | speedup | eficiencia |
|---|---:|---:|---:|---:|---:|
| secuencial | 20 | 0.150351 | 0.084601 | 1.00 | 1.00 |
| pthreads-4 | 20 | 0.041357 | 0.022899 | 3.64 | 0.91 |
| openmp-1 | 20 | 0.151284 | 0.085542 | 0.99 | 0.99 |
| openmp-2 | 20 | 0.080523 | 0.044694 | 1.87 | 0.93 |
| openmp-4 | 20 | 0.042719 | 0.022832 | 3.52 | 0.88 |
| openmp-12 | 20 | 0.031783 | 0.014809 | 4.73 | 0.39 |
| mpi-1 | 20 | 0.157976 | 0.087852 | 0.95 | 0.95 |
| mpi-2 | 20 | 0.079263 | 0.044050 | 1.90 | 0.95 |
| mpi-4 | 20 | 0.044541 | 0.024924 | 3.38 | 0.84 |

(La desviación estándar de esta tabla mezcla la variación entre
repeticiones con la variación entre damma/sulfur y PGM/PPM, que tienen
tamaños distintos — por eso es grande. La tabla de la sección 4 la separa
por imagen/formato.)

## 4. Tabla detallada — por versión, imagen y formato

Promedio y desviación estándar de `filtrado_real_s` sobre las 5
repeticiones únicamente (sin mezclar imágenes). Tabla completa (incluye
`lectura_real_s` y `escritura_real_s` promedio) en
`results/resumen_detallado.csv`.

| versión | imagen | formato | n | filtrado real: media (s) | filtrado real: std (s) |
|---|---|---|---:|---:|---:|
| secuencial | damma | pgm | 5 | 0.092608 | 0.000930 |
| secuencial | damma | ppm | 5 | 0.272888 | 0.002718 |
| secuencial | fruit | pgm | 5 | 0.029392 | 0.000335 |
| secuencial | fruit | ppm | 5 | 0.086947 | 0.001295 |
| secuencial | lena | pgm | 5 | 0.019212 | 0.000409 |
| secuencial | lena | ppm | 5 | 0.003600 | 0.000246 |
| secuencial | puj | pgm | 5 | 0.083638 | 0.000794 |
| secuencial | puj | ppm | 5 | 0.249621 | 0.005380 |
| secuencial | sulfur | pgm | 5 | 0.059900 | 0.000602 |
| secuencial | sulfur | ppm | 5 | 0.176008 | 0.001529 |
| pthreads-4 | damma | pgm | 5 | 0.026170 | 0.000675 |
| pthreads-4 | damma | ppm | 5 | 0.074902 | 0.002302 |
| pthreads-4 | sulfur | pgm | 5 | 0.016954 | 0.000191 |
| pthreads-4 | sulfur | ppm | 5 | 0.047402 | 0.000384 |
| openmp-1 | damma | pgm | 5 | 0.092780 | 0.000640 |
| openmp-1 | damma | ppm | 5 | 0.275536 | 0.002222 |
| openmp-1 | sulfur | pgm | 5 | 0.060199 | 0.000663 |
| openmp-1 | sulfur | ppm | 5 | 0.176621 | 0.001657 |
| openmp-2 | damma | pgm | 5 | 0.049160 | 0.000975 |
| openmp-2 | damma | ppm | 5 | 0.144894 | 0.000921 |
| openmp-2 | sulfur | pgm | 5 | 0.033167 | 0.003124 |
| openmp-2 | sulfur | ppm | 5 | 0.094873 | 0.002648 |
| openmp-4 | damma | pgm | 5 | 0.026147 | 0.000473 |
| openmp-4 | damma | ppm | 5 | 0.075380 | 0.002107 |
| openmp-4 | sulfur | pgm | 5 | 0.019065 | 0.001266 |
| openmp-4 | sulfur | ppm | 5 | 0.050283 | 0.004703 |
| openmp-12 | damma | pgm | 5 | 0.022894 | 0.003187 |
| openmp-12 | damma | ppm | 5 | 0.051968 | 0.002623 |
| openmp-12 | sulfur | pgm | 5 | 0.014881 | 0.002436 |
| openmp-12 | sulfur | ppm | 5 | 0.037389 | 0.003784 |
| mpi-1 | damma | pgm | 5 | 0.099406 | 0.009621 |
| mpi-1 | damma | ppm | 5 | 0.286321 | 0.004495 |
| mpi-1 | sulfur | pgm | 5 | 0.064422 | 0.004577 |
| mpi-1 | sulfur | ppm | 5 | 0.181753 | 0.000952 |
| mpi-2 | damma | pgm | 5 | 0.049735 | 0.001711 |
| mpi-2 | damma | ppm | 5 | 0.143108 | 0.001485 |
| mpi-2 | sulfur | pgm | 5 | 0.031842 | 0.000568 |
| mpi-2 | sulfur | ppm | 5 | 0.092365 | 0.000997 |
| mpi-4 | damma | pgm | 5 | 0.028611 | 0.003328 |
| mpi-4 | damma | ppm | 5 | 0.080501 | 0.003350 |
| mpi-4 | sulfur | pgm | 5 | 0.017310 | 0.000895 |
| mpi-4 | sulfur | ppm | 5 | 0.051744 | 0.001482 |

## 5. Gráficas (`results/graficas/`)

- `01_filtrado_por_version.png` — tiempo de filtrado real por versión
  (promedio ± desviación estándar), datos de la sección 3.
- `02_speedup.png` — speedup del filtrado por número de hilos/nodos
  (pthreads, OpenMP, MPI), con una línea de referencia de speedup ideal.
- `03_eficiencia.png` — eficiencia (speedup / hilos_o_nodos) por número de
  hilos/nodos, con una línea de referencia en 1.0.
- `04_desglose_io.png` — barras apiladas lectura/filtrado/escritura por
  versión (secuencial, pthreads-4, openmp-1/2/4/12; MPI no está porque
  `mpi_filterer` no mide lectura/escritura por separado), promediado sobre
  todas las imágenes aplicables de cada versión.

## 6. Archivos generados

```
results/
├── tiempos.csv              (591 líneas: 1 encabezado + 590 filas de datos crudas, normalizadas)
├── resumen.csv              (tabla de la sección 3)
├── resumen_detallado.csv    (tabla de la sección 4, incluye lectura/escritura)
└── graficas/
    ├── 01_filtrado_por_version.png
    ├── 02_speedup.png
    ├── 03_eficiencia.png
    └── 04_desglose_io.png
```

## 7. Cómo reproducir

```bash
source /etc/profile.d/modules.sh && module load mpi/openmpi-x86_64   # si hace falta mpic++/mpirun en el PATH
scripts/benchmark.sh                  # genera results/tiempos.csv

python3 -m venv .venv                 # una sola vez
.venv/bin/pip install matplotlib pandas numpy
.venv/bin/python scripts/graficar.py  # genera results/graficas/ y results/resumen*.csv
```
