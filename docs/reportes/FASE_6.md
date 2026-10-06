# Fase 6 — Ajustes antes del informe

Fecha: 2026-10-05
Rama: `resultados` (con el fix de la sección 1 propagado desde `diseno-1`
a través de `diseno-2`, `diseno-3`, `diseno-4`)

## 1. Anomalía de escritura: investigación, causa y corrección

### 1.1 El problema

En `docs/reportes/FASE_5.md`, `escritura_real_s` de `th_filterer`/
`omp_filterer` era ~25-30% más alta que la de `filterer` para la misma
imagen (ej. damma.pgm: ~0.143s secuencial vs ~0.184s pthreads), **pese a
que los tres ejecutables llaman exactamente la misma función**
`ImagenIO::escribir()` (confirmado leyendo `filterer.cpp`/`th_filterer.cpp`/
`omp_filterer.cpp`: los tres son una línea que llama a `ejecutarCLI()`, que
a su vez llama siempre a la misma `ImagenIO::escribir`). No podía ser un
bug algorítmico/de código — tenía que ser un efecto de runtime.

### 1.2 Hipótesis descartadas (con evidencia)

Se reprodujo primero el problema de forma controlada (10 repeticiones,
`damma.pgm`, filtro `blur`, antes de tocar nada):

```
filterer:      ~0.048s        th_filterer: ~0.062s        omp_filterer: ~0.061s
```
Reproducible y estable (~27-29% de diferencia), no era ruido.

- **Frecuencia de CPU durante la escritura** (`scripts/diagnostico_frecuencia.py`,
  muestrea `scaling_cur_freq` de los 12 núcleos cada 4ms y lo correlaciona
  con las ventanas de lectura/filtrado/escritura que reporta el propio
  programa): la frecuencia promedio durante la escritura fue **similar o
  incluso más alta** en `th_filterer`/`omp_filterer` (3.77-3.92 GHz) que en
  `filterer` (3.74 GHz), y la frecuencia máxima de un núcleo fue ~4.12-4.13
  GHz en los tres casos. **Descartada**: la CPU no estaba más lenta durante
  la escritura en las versiones paralelas.
- **`OMP_WAIT_POLICY=passive`** (hilos OpenMP inactivos duermen en vez de
  hacer *busy-wait*): sin diferencia (`omp_filterer` con `passive`:
  ~0.060s; con `active`, el valor por defecto: ~0.060s). Además, esta
  hipótesis no podía explicar por qué `th_filterer` —que no usa el runtime
  de OpenMP en absoluto— mostraba el mismo problema. **Descartada**.
- **Arenas de malloc de glibc** (`MALLOC_ARENA_MAX=1`, por si la creación
  de hilos fragmentaba el heap en arenas separadas y eso afectaba la
  asignación de memoria durante la escritura): sin diferencia (~0.06s en
  ambos ejecutables, igual que sin la variable). **Descartada**.
- **Migración/afinidad entre núcleos** (`taskset -c 0`, forzando *todo* el
  proceso —hilos de pthreads incluidos— a un único núcleo físico, para
  descartar que el problema fuera tráfico de coherencia de caché entre
  núcleos distintos): el problema **persistió igual** (`th_filterer` con
  `taskset -c 0`: ~0.060-0.063s; `filterer` con `taskset -c 0`, mismo
  núcleo: ~0.048-0.051s). **Descartada**: no es un efecto de caché entre
  núcleos, porque ocurre incluso forzando todo a un solo núcleo.
- `perf stat`: no está instalado en esta máquina (`which perf` no lo
  encuentra) y, aunque lo estuviera, `/proc/sys/kernel/perf_event_paranoid`
  vale `2` (bloquea el acceso a contadores de hardware sin privilegios).
  No se usó, tal como admite la consigna ("si está disponible sin sudo").

### 1.3 Causa confirmada: `pthread_create()` desactiva permanentemente un *fast path* de `<cstdio>` en glibc

Se aisló la causa con un microbenchmark mínimo, sin nada de nuestro código
(`/tmp/diag/prueba_single_threaded.cpp`, glibc 2.42 en esta máquina): un
bucle de 2 millones de `fprintf`+`fputc`, comparando "sin crear ningún
hilo" vs "crear y unir (`pthread_join`) un hilo vacío **antes** del
bucle, que no hace nada":

```
sin pthread_create:  ~0.0742s  (promedio de 5 corridas)
con pthread_create:  ~0.0945s  (promedio de 5 corridas)   -> +27%
```

Mismo orden de magnitud y mismo signo que la anomalía real. La causa: en
glibc, las funciones de `<cstdio>` (`fprintf`, `fputc`, etc.) toman un
*lock* interno sobre el `FILE*` para ser seguras entre hilos. Si el
proceso nunca ha creado un hilo, glibc sabe que es imposible que haya
concurrencia y usa una ruta rápida sin ese *lock* (expuesta internamente
vía `__libc_single_threaded`). **En cuanto el proceso llama a
`pthread_create()` una sola vez —aunque el hilo termine de inmediato y se
una con `pthread_join()`— esa bandera queda desactivada para el resto de
la vida del proceso**, y todas las llamadas a `<cstdio>` posteriores pagan
el costo completo del *lock*, sin importar que ya no haya ningún otro hilo
corriendo.

Esto explica exactamente el patrón observado:
- **`th_filterer` y `omp_filterer` lo sufren por igual** porque ambos
  llaman a `pthread_create()` al menos una vez (directamente en
  `EjecutorPthreads`, o indirectamente vía el *thread pool* de libgomp que
  arma `#pragma omp parallel`), sin importar que la hipótesis fuera
  específica de OpenMP o no.
- **La lectura nunca se ve afectada** (confirmado también en los datos de
  `docs/reportes/FASE_5.md`: `lectura_real_s` era prácticamente igual en
  secuencial/pthreads/OpenMP para la misma imagen) porque `ImagenIO::leer()`
  corre en `main()` **antes** de que `ejecutarCLI()` invoque al `Ejecutor`
  que crea los hilos — el proceso todavía es monohilo en ese punto.
- **La escritura sí se ve afectada** porque corre **después** del
  filtrado paralelo, cuando el proceso ya quedó marcado como
  "multihilo para siempre", aunque los hilos de trabajo ya hayan
  terminado.
- El efecto es medible porque el código original hacía **un `fprintf` +
  un `fputc` por cada valor de píxel** (cientos de miles a millones de
  llamadas a `<cstdio>` por ejecución), así que el costo extra por llamada
  se acumulaba.

### 1.4 Corrección aplicada

No es un bug de corrección (la salida era —y sigue siendo— byte idéntica),
pero sí una ineficiencia real y corregible: **`ImagenIO::escribir()`**
(`src/ImagenIO.cpp`) ahora arma el texto de cada **fila** en un arreglo
dinámico y la escribe con un único `fwrite()`, en vez de un
`fprintf()`+`fputc()` por cada valor. Esto baja las llamadas a `<cstdio>`
de "cantidad de píxeles" a "alto de la imagen", haciendo el costo del
*lock* irrelevante para cualquier versión (y de paso acelera la escritura
en general, no solo en las versiones paralelas).

Aplicado en `diseno-1` (donde vive `ImagenIO`) y propagado con
`git merge` a `diseno-2` → `diseno-3` → `diseno-4` → `resultados`, sin
conflictos. Verificado en cada paso:
- `processor`: las 14 imágenes de prueba, verificadas por valores
  numéricos — 0 fallas, formato de texto idéntico al anterior.
- `filterer`/`th_filterer`/`omp_filterer`: `cmp` byte a byte contra la
  salida anterior para damma/sulfur (pgm/ppm) × 4 filtros — 0 fallas.
- `mpi_filterer`: `cmp` contra la secuencial — sin fallas (sección 2).

**Resultado tras el fix** (mismas 10 repeticiones, `damma.pgm`, `blur`):

```
filterer:      ~0.040s        th_filterer: ~0.041s        omp_filterer: ~0.041s
```

La brecha desapareció (las tres quedan dentro del mismo rango, solapando),
y la escritura quedó más rápida para **todas** las versiones (no solo las
paralelas): ~0.048s → ~0.040s en `filterer`, ~0.062s → ~0.041s en
`th_filterer`/`omp_filterer`.

## 2. MPI en Docker: instrumentación y benchmark dentro del clúster

### 2.1 Instrumentación agregada a `mpi_filterer` (rama `diseno-4`)

El rank 0 ahora también mide e imprime `lectura_real_s`/`lectura_cpu_s`,
`escritura_real_s`/`escritura_cpu_s` (sumada entre los 3 archivos de
salida, mismo criterio que las demás versiones) y `total_real_s`/
`total_cpu_s`. Los demás ranks no leen ni escriben archivo, así que esas
columnas quedan vacías en su línea del CSV. El encabezado nuevo:

```
rank,size,entrada,ancho,alto,filas_propias,lectura_real_s,lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,comunicacion_real_s,comunicacion_cpu_s,escritura_real_s,escritura_cpu_s,total_real_s,total_cpu_s
```

Verificado con `cmp` (salida idéntica a la secuencial) y con `valgrind
--leak-check=full` sobre `mpirun -np 1 ./mpi_filterer`: **0 bytes
"definitely lost" que pasen por nuestro código** (se revisaron las 37
trazas de error reportadas; todas están dentro de la inicialización
interna de OpenMPI/PMIx/UCX/PSM3 — el mismo patrón de falso positivo ya
documentado con `libgomp` en `docs/reportes/FASE_3.md`, no una fuga
introducida por este cambio).

### 2.2 Benchmark dentro del clúster Docker

`scripts/benchmark_mpi_docker.sh` (+ `scripts/benchmark_mpi_docker_extra.sh`
para lena/fruit/puj) corre, **dentro del contenedor `maestro`**, vía
`mpirun --hostfile hostfile --bind-to none`, usando los 4 contenedores
reales (SSH entre ellos, igual que en la Fase 4):

- damma, sulfur (PGM y PPM): 1, 2 y 4 nodos, 5 repeticiones cada uno.
- lena, fruit, puj (PGM y PPM): 4 nodos, 5 repeticiones cada uno (sección 3).

Resultado: `results/tiempos_mpi_docker.csv` (261 líneas: 1 encabezado +
260 filas). Verificado con `cmp` que la salida del clúster Docker sigue
siendo idéntica a la secuencial (ej. `sulfur.ppm` con 4 nodos: blur,
laplace y sharpen, los 3 idénticos).

Las mediciones **locales** de MPI (sin Docker, `mpirun` directo en el
host) se conservan en `results/tiempos.csv` (`programa=mpi`) para poder
comparar local vs. Docker — ver tabla de la sección 4b y la gráfica
`05_speedup_vs_pixeles.png` (incluye ambas series, "mpi-4 (local)" y
"mpi-4 (docker)").

## 3. Impacto del tamaño de la imagen

`scripts/benchmark.sh` se extendió (y se corrió de nuevo, completo, ya con
el fix de la sección 1 y con el sistema en reposo — ver sección 5) para
que `th_filterer` y `omp_filterer` corran también sobre **lena, fruit y
puj**, no solo damma/sulfur, y para que OpenMP use `--hilos {1,2,4,6,12}`
(se agregó 6 al conjunto original `{1,2,4,12}` de la Fase 5). Sumado a los
4 nodos de MPI en Docker de la sección 2.2, las **5 imágenes de prueba
quedan cubiertas en las 4 versiones**.

`results/tiempos.csv` quedó con 1191 líneas (1190 filas de datos):
secuencial (5 imágenes × 2 formatos × 5 repeticiones = 50 ejecuciones, 150
filas), pthreads (50 ejecuciones, 150 filas), OpenMP (5 imágenes × 2
formatos × 5 hilos × 5 repeticiones = 250 ejecuciones, 750 filas), MPI
local (damma/sulfur × 2 formatos × 3 nodos × 5 repeticiones = 60
ejecuciones, 140 filas).

Gráfica: `results/graficas/05_speedup_vs_pixeles.png` — speedup del
filtrado (eje Y) vs. número de píxeles de la imagen (ancho × alto ×
canales, eje X logarítmico), para pthreads-4, openmp-12, mpi-4 (local) y
mpi-4 (docker), sobre las 5 imágenes (PGM y PPM, 10 puntos por versión).

## 4. Tablas

### (a) CPU/real del filtrado por versión

Promedio sobre damma+sulfur, PGM+PPM, 5 repeticiones (20 muestras por
versión), excepto `mpi-docker-*` que son exactamente esas mismas 20
muestras pero corridas dentro del clúster Docker. Tabla completa en
`results/resumen_cpu_real.csv`.

| versión | n | filtrado real: media (s) | filtrado CPU: media (s) | razón CPU/real |
|---|---:|---:|---:|---:|
| secuencial | 20 | 0.147162 | 0.146598 | 1.00 |
| pthreads-4 | 20 | 0.039892 | 0.156332 | 3.92 |
| openmp-1 | 20 | 0.147781 | 0.147152 | 1.00 |
| openmp-2 | 20 | 0.075593 | 0.149174 | 1.97 |
| openmp-4 | 20 | 0.039236 | 0.151137 | 3.85 |
| openmp-6 | 20 | 0.026976 | 0.151248 | 5.61 |
| openmp-12 | 20 | 0.024312 | 0.256207 | 10.54 |
| mpi-1 | 20 | 0.151591 | 0.151031 | 1.00 |
| mpi-2 | 20 | 0.077174 | 0.076898 | 1.00 |
| mpi-4 | 20 | 0.039584 | 0.039443 | 1.00 |
| mpi-docker-1 | 20 | 0.156016 | 0.155423 | 1.00 |
| mpi-docker-2 | 20 | 0.079583 | 0.079206 | 1.00 |
| mpi-docker-4 | 20 | 0.040855 | 0.040700 | 1.00 |

Nota de lectura (sin interpretar el *por qué*, solo aclarando qué mide la
columna): en pthreads/OpenMP la razón CPU/real sube con el número de
hilos porque `clock()` suma el tiempo de CPU de todos los hilos del
proceso (documentado también en `docs/reportes/FASE_3.md`); en MPI cada
rank es un proceso separado con su propio `clock()`, así que la razón se
mantiene cerca de 1.00 incluso con más nodos (el tiempo de CPU medido es
el del propio rank 0, no la suma de todos los ranks).

### (b) Tiempo total y speedup total

`total_real_s` = lectura + filtrado + escritura + comunicación (0 cuando
no aplica). `speedup_total` se calcula contra el `total_real_s` promedio
de `secuencial`; `speedup_filtrado` se incluye al lado para comparar
(tabla completa, con desviación estándar, en `results/resumen_total.csv`).

| versión | n | total real: media (s) | speedup total | speedup filtrado |
|---|---:|---:|---:|---:|
| secuencial | 20 | 0.443916 | 1.00 | 1.00 |
| pthreads-4 | 20 | 0.337561 | 1.32 | 3.69 |
| openmp-1 | 20 | 0.454823 | 0.98 | 1.00 |
| openmp-2 | 20 | 0.383043 | 1.16 | 1.95 |
| openmp-4 | 20 | 0.347687 | 1.28 | 3.75 |
| openmp-6 | 20 | 0.335220 | 1.32 | 5.46 |
| openmp-12 | 20 | 0.340146 | 1.31 | 6.05 |
| mpi-1 | 20 | 0.477841 | 0.93 | 0.97 |
| mpi-2 | 20 | 0.407042 | 1.09 | 1.91 |
| mpi-4 | 20 | 0.376550 | 1.18 | 3.72 |
| mpi-docker-1 | 20 | 0.545229 | 0.81 | 0.94 |
| mpi-docker-2 | 20 | 0.476021 | 0.93 | 1.85 |
| mpi-docker-4 | 20 | 0.450373 | 0.99 | 3.60 |

Gráfica: `results/graficas/06_speedup_filtrado_vs_total.png` (dispersión
de speedup de filtrado vs. speedup total por versión, con la línea
`y=x` de referencia).

### (c) Fracción secuencial medida y speedup máximo teórico (Ley de Amdahl)

**Dos análisis distintos.** El primero (c.1) mira *solo* el kernel de
filtrado en aislamiento; el segundo (c.2) mira el **programa completo**
(lectura + filtrado + escritura [+ comunicación en MPI]), que es el que
importa para responder "¿qué tanto mejora realmente correr esto en
paralelo?". **El speedup máximo de 35x-39x de la tabla (c.1) aplica
únicamente al kernel de filtrado aislado, no al programa**: en cuanto se
cuenta la E/S (que esta implementación no paraleliza), el límite real baja
a ~1.50x (tabla c.2). Corrección sobre la versión original de este reporte,
que solo incluía (c.1) y podía leerse como si ese límite aplicara al
programa completo.

#### (c.1) Solo el kernel de filtrado

Se despeja la fracción no paralelizable `f` de la fórmula de Amdahl,
`speedup = 1 / (f + (1-f)/n)`, usando el **speedup del filtrado** medido
con el mayor paralelismo probado de cada versión (`n_usado`):
`f = (1/speedup - 1/n) / (1 - 1/n)`. El "speedup máximo teórico" es el
límite de Amdahl cuando `n -> infinito`, es decir `1/f`. Tabla completa en
`results/resumen_amdahl.csv`.

| versión | n usado | speedup medido (filtrado) | f (fracción secuencial del filtrado) | speedup máximo teórico del FILTRADO (n→∞) |
|---|---:|---:|---:|---:|
| pthreads | 4 | 3.69 | 0.0281 | 35.58 |
| openmp | 12 | 6.05 | 0.0893 | 11.20 |
| mpi (local) | 4 | 3.72 | 0.0253 | 39.51 |
| mpi (docker) | 4 | 3.60 | 0.0368 | 27.15 |

#### (c.2) Programa completo (lectura + filtrado + escritura [+ comunicación])

Aquí `p` es la fracción **paralelizable del programa secuencial**, medida
directamente (no despejada): `p = filtrado_secuencial / total_secuencial`
(promedio damma+sulfur, PGM+PPM) = **0.3315** (el filtrado es ~1/3 del
tiempo total; el resto —lectura y escritura de texto plano— es la parte
que esta implementación no paraleliza). El límite de Amdahl para el
programa completo, `n -> infinito`, es:

```
speedup_máximo_programa = 1 / (1 - p) = 1 / (1 - 0.3315) = 1.496x
```

Es decir: **por más hilos, núcleos o nodos que se agreguen, este programa
nunca va a ir más de ~1.5x más rápido de punta a punta**, porque ~67% del
tiempo (lectura + escritura de texto plano) sigue siendo secuencial sin
importar cuánto se paralelice el filtrado — consistente con lo ya
señalado en `docs/reportes/FASE_2.md` sobre el peso de la E/S.

El speedup total **predicho** por Amdahl a cada `n` usa el speedup de
filtrado **realmente medido** a ese `n` (no un `n` ideal) como estimador
de qué tan rápida queda la parte paralela:
`speedup_total_predicho = 1 / ((1-p) + p / speedup_filtrado_medido)`.
Tabla completa en `results/resumen_amdahl_programa.csv`; gráfica en
`results/graficas/07_amdahl_predicho_vs_medido.png`.

| versión | speedup filtrado medido | speedup total predicho (Amdahl) | speedup total medido |
|---|---:|---:|---:|
| pthreads-4 | 3.69 | 1.32 | 1.32 |
| openmp-2 | 1.95 | 1.19 | 1.16 |
| openmp-4 | 3.75 | 1.32 | 1.28 |
| openmp-6 | 5.46 | 1.37 | 1.32 |
| openmp-12 | 6.05 | 1.38 | 1.31 |
| mpi-2 (local) | 1.91 | 1.19 | 1.09 |
| mpi-4 (local) | 3.72 | 1.32 | 1.18 |
| mpi-docker-2 | 1.85 | 1.18 | 0.93 |
| mpi-docker-4 | 3.60 | 1.31 | 0.99 |

El modelo de Amdahl predice razonablemente bien a pthreads/OpenMP (medido
dentro de ~0.03-0.05x del predicho). Para MPI —sobre todo MPI en Docker—
el medido queda más por debajo del predicho (p. ej. mpi-docker-4: 0.99x
medido vs 1.31x predicho), porque el modelo de Amdahl de esta tabla no
incluye el costo de comunicación por separado (solo usa el speedup de
filtrado); en MPI ese costo adicional sí se paga dentro del `total_real_s`
medido. Ningún caso supera el límite teórico de 1.496x.

## 5. Carga del sistema al momento de medir

Como pedía la consigna, antes de correr los benchmarks de las secciones 2
y 3 se le pidió al usuario cerrar Chrome, Discord y Spotify:

```
Antes (Fase 5, sin cerrar nada):  load average: 1.80, 1.69, 1.47
Después de cerrar las apps:       load average: 0.59, 1.07, 1.21
```

`ps aux` seguía mostrando un par de procesos de Chrome (probablemente
subprocesos zygote/GPU que tardan en terminar); no se forzó su cierre.
El `load average` de 1 minuto bajó de 1.80 a 0.59 (-67%), una mejora
clara aunque no perfecta. Todos los benchmarks de las secciones 2, 3 y 4
de este reporte (`results/tiempos.csv` regenerado y
`results/tiempos_mpi_docker.csv`) se corrieron después de este punto.

## 6. Comparaciones visuales (`results/visual/`)

Para `fruit.ppm` y `sulfur.pgm`, original + los 4 filtros (blur, laplace,
sharpen, sobel), generados con `filterer` (secuencial) y convertidos a PNG
con ImageMagick:

```
results/visual/
├── fruit_original.png / fruit_blur.png / fruit_laplace.png / fruit_sharpen.png / fruit_sobel.png
├── fruit_comparacion.png      (montaje de las 5 anteriores, lado a lado)
├── sulfur_original.png / sulfur_blur.png / sulfur_laplace.png / sulfur_sharpen.png / sulfur_sobel.png
├── sulfur_comparacion.png     (montaje de las 5 anteriores, lado a lado)
├── fruit_zoom_original.png / fruit_zoom_blur.png / fruit_zoom_sharpen.png
│                             (recorte 160x160 de la papaya de fruit.ppm, con zoom 3x
│                              -filtro "point", sin interpolar, para no disimular el blur-)
└── fruit_zoom_comparacion.png (montaje de los 3 anteriores, lado a lado)
```

El recorte con zoom (sección añadida en la Fase 7) hace visible lo que en
la imagen completa se nota poco: el blur suaviza claramente los bordes de
las semillas de la papaya, y el sharpen realza el contraste de esos mismos
bordes frente al original.

## 7. Archivos nuevos/modificados de esta fase

```
src/ImagenIO.cpp                        (fix, diseno-1 -> propagado)
src/mpi_filterer.cpp                    (instrumentación lectura/escritura/total, diseno-4)
scripts/diagnostico_frecuencia.py       (investigación, sección 1)
scripts/benchmark.sh                    (extendido: 5 imágenes en pthreads/OpenMP, hilos +6)
scripts/benchmark_mpi_docker.sh         (nuevo: MPI dentro de Docker, damma/sulfur)
scripts/benchmark_mpi_docker_extra.sh   (nuevo: MPI dentro de Docker, lena/fruit/puj)
scripts/graficar.py                     (agregación de mpi actualizada al nuevo esquema CSV)
scripts/graficar_fase6.py               (tablas 4a/4b/4c -filtrado y programa- y gráficas 05/06/07)
results/tiempos.csv                     (regenerado completo, post-fix, sistema en reposo)
results/tiempos_mpi_docker.csv          (nuevo)
results/resumen_cpu_real.csv            (nuevo, tabla 4a)
results/resumen_total.csv               (nuevo, tabla 4b)
results/resumen_amdahl.csv              (nuevo, tabla 4c.1, solo filtrado)
results/resumen_amdahl_programa.csv     (nuevo, tabla 4c.2, programa completo)
results/graficas/05_speedup_vs_pixeles.png     (nuevo)
results/graficas/06_speedup_filtrado_vs_total.png (nuevo)
results/graficas/07_amdahl_predicho_vs_medido.png (nuevo)
results/visual/                         (nuevo, incluye recortes con zoom)
```

## 8. Pendientes / dudas

- `perf stat` no se pudo usar (no instalado, y `perf_event_paranoid=2`
  bloquearía el acceso sin privilegios de todas formas); la causa de la
  sección 1 quedó confirmada igual con el microbenchmark aislado, que es
  evidencia más directa que contadores de hardware para este caso puntual.
- El `load average` no bajó a 0 tras pedir cerrar las apps (seguían
  algunos procesos de Chrome); se documentó el número real medido, no uno
  ideal.
- Queda pendiente, como ya se anotó en `docs/reportes/FASE_4.md`, el
  informe final en PDF y el video — no son código, van aparte.
