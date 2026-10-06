# Fase 3 — Diseño 3: Memoria compartida (`th_filterer`, `omp_filterer`)

Fecha: 2026-10-05
Rama: `diseno-3` (parte de `diseno-2`)
Máquina de pruebas: AMD Ryzen 5 5600H, **12 núcleos lógicos** (6 núcleos
físicos x 2 hilos SMT, `nproc --all` → 12).

## 1. Qué se hizo

Se agregaron dos ejecutables nuevos, `th_filterer` (pthreads) y
`omp_filterer` (OpenMP), que paralelizan el mismo filtrado del diseño 2
**sin reimplementar ninguna convolución**. Para lograr eso sin duplicar
código entre `filterer`/`th_filterer`/`omp_filterer`, se introdujo una capa
de estrategia:

```
include/EjecutorFiltro.h       -> interfaz: ejecutar(filtro, entrada, salida)
include/EjecutorSecuencial.h    -> una llamada a Filtro::aplicar sobre toda la imagen
include/EjecutorPthreads.h      -> 4 hilos, uno por cuadrante
include/EjecutorOpenMP.h        -> "#pragma omp parallel for" por filas
include/EjecucionCLI.h          -> CLI + medición + CSV, compartido por los 3 ejecutables
```

Cada `Ejecutor*` decide **cómo particionar el trabajo**, pero el cálculo de
cada píxel sigue viviendo exclusivamente en `Filtro::aplicar` (diseño 2),
invocado con distintas regiones `[y0,y1)x[x0,x1)`. `EjecucionCLI.cpp`
contiene la lógica de parseo de `--f`, lectura, medición por fases y
generación de las salidas `_blur`/`_laplace`/`_sharpen` que antes vivía
duplicada solo en `filterer.cpp`; ahora los tres ejecutables
(`filterer.cpp`, `th_filterer.cpp`, `omp_filterer.cpp`) son prácticamente
una línea: construyen su `Ejecutor*` concreto y llaman a
`ejecutarCLI(argc, argv, ejecutor)`. Esto significa que **`filterer.cpp`
se refactorizó** (su comportamiento observable no cambia, salvo que el CSV
ahora incluye una columna `ejecutor` al inicio, útil para comparar
secuencial/pthreads/OpenMP en la misma tabla).

### A) `th_filterer` (pthreads, 4 cuadrantes)

`EjecutorPthreads::ejecutar()` (`src/EjecutorPthreads.cpp`) divide la
imagen en 4 regiones con división entera:

```cpp
int midX = ancho / 2;
int midY = alto / 2;
// arriba-izq:  [0,midY) x [0,midX)
// arriba-der:  [0,midY) x [midX,ancho)
// abajo-izq:   [midY,alto) x [0,midX)
// abajo-der:   [midY,alto) x [midX,ancho)
```

y lanza un `pthread_create` por cuadrante, cada uno ejecutando
`filtro.aplicar(entrada, salida, y0,y1,x0,x1)` sobre su región; luego
`pthread_join` a los 4. **Dimensiones impares**: con división entera, un
cuadrante queda con una fila/columna más que el opuesto (p. ej.
`sulfur` es 823 de ancho: `midX=411`, el cuadrante izquierdo cubre
columnas 0-410 = 411 columnas, el derecho 411-822 = 412 columnas), pero la
unión de los 4 rangos sigue cubriendo exactamente `[0,ancho)x[0,alto)` sin
huecos ni solapes — no se pierde ni se duplica ningún píxel. Se verificó
con `damma` (1000x1278, ancho par/alto par) y `sulfur` (823x1000, ancho
impar/alto par) y con `feep` (24x7, ambos... 24 par, 7 impar) en las
pruebas de humo.

### B) `omp_filterer` (OpenMP, filas)

`EjecutorOpenMP::ejecutar()` (`src/EjecutorOpenMP.cpp`):

```cpp
if (numHilos > 0) omp_set_num_threads(numHilos);
#pragma omp parallel for schedule(static)
for (int y = 0; y < alto; y++) {
    filtro.aplicar(entrada, salida, y, y + 1, 0, ancho);
}
```

Cada iteración del `for` es una fila completa (`[y,y+1)x[0,ancho)`), otra
vez delegando a `Filtro::aplicar`. El número de hilos es configurable con
`--hilos N` en la línea de comandos (`src/omp_filterer.cpp` lo parsea antes
de construir el `EjecutorOpenMP`); si no se pasa `--hilos`, se usa
`OMP_NUM_THREADS` del entorno, o si tampoco está definida, el valor por
defecto del runtime de OpenMP (normalmente el número de núcleos lógicos).
Sin `--f`, igual que `filterer`/`th_filterer`, se aplican los tres filtros
obligatorios y se generan `_blur`/`_laplace`/`_sharpen`.

## 2. Por qué no hay condiciones de carrera (y no hace falta ningún mutex)

Esto aplica igual a `th_filterer` y a `omp_filterer`:

- **`entrada` es de solo lectura y compartida.** Ningún hilo escribe en
  ella. `Filtro::aplicar`/`Filtro::convolucionar` reciben `const Imagen&`
  y solo llaman a `obtenerValor()` (nunca `asignarValor()`) sobre
  `entrada` — el compilador ya impide escribirla por el `const`. Que un
  hilo lea, para resolver sus vecinos de borde, posiciones de `entrada`
  que están físicamente fuera de su propia región (p. ej. el cuadrante
  arriba-izquierda lee la columna `midX` para sus vecinos del borde
  derecho) no es un problema: son lecturas concurrentes del mismo dato
  inmutable, y leer concurrentemente sin escribir nunca es seguro.
- **Cada hilo/iteración escribe solo en su propia región de `salida`,
  y esas regiones son disjuntas por construcción.** En `th_filterer` los 4
  cuadrantes particionan `[0,ancho)x[0,alto)` sin solape (sección 1A). En
  `omp_filterer` cada iteración del `parallel for` escribe una fila `y`
  distinta; dos iteraciones nunca tocan la misma fila. Como cada posición
  de memoria de `salida` la escribe como máximo un hilo, no existe
  condición de carrera de escritura-escritura ni de lectura-escritura
  sobre `salida`.
- **No hay ninguna variable compartida mutable fuera de `salida`.** No se
  usa ningún acumulador global, contador compartido ni estructura
  intermedia entre hilos (a diferencia de, por ejemplo, un `reduction` de
  OpenMP, que aquí no hace falta porque no se calcula ningún agregado
  entre filas/cuadrantes).

Por estas tres razones —entrada inmutable compartida, escritura en
regiones disjuntas, sin estado mutable compartido adicional— el filtrado
paralelo es seguro sin un solo `mutex`, `std::atomic` ni sección crítica.
Esto es exactamente lo que anticipaba `CLAUDE.md` al pedir que
`Filtro::aplicar` reciba una región explícita.

**Verificación con herramientas, no solo argumento**: se corrió
`valgrind --tool=helgrind` (detector de condiciones de carrera) sobre
`th_filterer`:
```
$ valgrind --tool=helgrind --error-exitcode=1 ./th_filterer images/feep.pgm out.pgm --f blur
==67370== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 11 from 3)
```
0 errores (las 11 supresiones son las del propio runtime de pthreads/glibc,
no de nuestro código). Confirma la ausencia de carreras, no solo el
razonamiento de diseño.

## 3. Por qué el tiempo de CPU puede superar al tiempo real

`Temporizador::segundosCPU()` usa `clock()`, que mide el tiempo de CPU
consumido por **todo el proceso**, sumando el tiempo de **todos los
hilos/núcleos** que estuvieron activos durante la medición — no es "tiempo
de CPU por hilo", es un acumulado. Si 4 hilos corren en paralelo durante 1
segundo de reloj real, cada uno usando ~1 segundo de CPU, `clock()` reporta
~4 segundos de tiempo de CPU aunque el reloj de pared solo avanzó 1
segundo. Por eso, en cuanto hay más de un hilo trabajando simultáneamente
en la fase de filtrado, es normal y esperado que `filtrado_cpu_s >
filtrado_real_s` (lo contrario de lo que pasaba en el diseño 2, secuencial
de un solo hilo, donde CPU ≈ real).

Se ve claramente en los datos de la sección 5: por ejemplo
`damma.pgm` blur con `pthreads-4-cuadrantes`:
`filtrado_real_s=0.027208` pero `filtrado_cpu_s=0.107485` (~3.95x,
coherente con 4 hilos ocupados casi todo ese tiempo). Con OpenMP a 12
hilos la brecha es aún mayor (p. ej. `damma.ppm` sobel a 12 hilos:
`filtrado_real_s=0.064750`, `filtrado_cpu_s=0.745965`, ~11.5x) porque hay
más hilos sumando tiempo de CPU al mismo acumulado, aunque el tiempo real
siga bajando. Esto **no es un error de medición**: es la definición misma
de tiempo de CPU acumulado en un programa con varios hilos activos, tal
como anticipa `CLAUDE.md`.

## 4. Verificación: salidas idénticas a la versión secuencial

Se filtraron `damma` y `sulfur` (PGM y PPM) con los 4 filtros
(blur/laplace/sharpen/sobel) usando `filterer`, `th_filterer` y
`omp_filterer`, y se comparó cada salida contra la de `filterer` con
`cmp`:

```bash
for img in damma sulfur; do
  for ext in pgm ppm; do
    for filtro in blur laplace sharpen sobel; do
      ./filterer     images/$img.$ext output/seq/${img}_${filtro}.$ext --f $filtro
      ./th_filterer  images/$img.$ext output/th/${img}_${filtro}.$ext  --f $filtro
      ./omp_filterer images/$img.$ext output/omp/${img}_${filtro}.$ext --f $filtro --hilos 12
      cmp output/seq/${img}_${filtro}.$ext output/th/${img}_${filtro}.$ext
      cmp output/seq/${img}_${filtro}.$ext output/omp/${img}_${filtro}.$ext
    done
  done
done
```
**32/32 comparaciones (`th_filterer` y `omp_filterer`, 16 combinaciones
imagen/formato/filtro cada uno) fueron `cmp` idénticas byte a byte** a la
salida secuencial. Además se repitió la comparación para `omp_filterer`
con 1, 2, 4 y 12 hilos (32 comparaciones adicionales: blur y sobel, damma y
sulfur, pgm y ppm, cada uno a 4 conteos de hilos): **0 fallas**. Esto era
esperable porque cada píxel de salida se calcula con la misma aritmética
de punto flotante en el mismo orden (la suma de los 9 términos del kernel
dentro de `convolucionar()`), sin importar qué hilo ni con cuántos hilos
se ejecute esa llamada — no hay reducción ni acumulación entre hilos que
pudiera introducir diferencias de redondeo.

## 5. Tiempos reales y speedup del filtrado

Todas las cifras son de ejecuciones reales en esta máquina (`make clean &&
make`, sin valgrind). El speedup se calcula sobre la **fase de filtrado**
únicamente (`filtrado_real_s`), que es la fase que realmente se paraleliza
(lectura/escritura siguen siendo secuenciales e iguales en los tres
ejecutables).

### 5.1 Secuencial vs. pthreads (4 cuadrantes) — damma y sulfur, 4 filtros

| imagen | filtro | filtrado secuencial (s) | filtrado pthreads (s) | speedup |
|---|---|---:|---:|---:|
| damma.pgm | blur | 0.1106 | 0.0272 | 4.06x |
| damma.pgm | laplace | 0.1206 | 0.0319 | 3.78x |
| damma.pgm | sharpen | 0.1118 | 0.0286 | 3.91x |
| damma.pgm | sobel | 0.1523 | 0.0386 | 3.94x |
| damma.ppm | blur | 0.3339 | 0.1063 | 3.14x |
| damma.ppm | laplace | 0.3584 | 0.0939 | 3.82x |
| damma.ppm | sharpen | 0.3329 | 0.0863 | 3.86x |
| damma.ppm | sobel | 0.4897 | 0.1652 | 2.96x |
| sulfur.pgm | blur | 0.0754 | 0.0192 | 3.92x |
| sulfur.pgm | laplace | 0.0829 | 0.0237 | 3.50x |
| sulfur.pgm | sharpen | 0.0737 | 0.0202 | 3.66x |
| sulfur.pgm | sobel | 0.0990 | 0.0243 | 4.08x |
| sulfur.ppm | blur | 0.2159 | 0.0731 | 2.95x |
| sulfur.ppm | laplace | 0.2347 | 0.0610 | 3.85x |
| sulfur.ppm | sharpen | 0.2256 | 0.0534 | 4.22x |
| sulfur.ppm | sobel | 0.2887 | 0.0790 | 3.65x |

Con 4 hilos fijos (uno por cuadrante) el speedup del filtrado está entre
**~2.96x y ~4.22x**, es decir, cerca del ideal de 4x que predice la parte
paralelizable según la ley de Amdahl para 4 unidades de trabajo del mismo
tamaño aproximado; se aleja algo del ideal cuando la imagen tiene
dimensiones muy asimétricas en relación al punto medio (afecta el balance
de carga entre los 4 cuadrantes) o cuando el sistema operativo no logra
mantener los 4 hilos corriendo en núcleos separados todo el tiempo.

### 5.2 OpenMP con 1, 2, 4 y 12 hilos — blur y sobel, damma y sulfur

| imagen | filtro | hilos | filtrado (s) | speedup vs. secuencial |
|---|---|---:|---:|---:|
| damma.pgm | blur | 1 | 0.1039 | 1.06x |
| damma.pgm | blur | 2 | 0.0533 | 2.07x |
| damma.pgm | blur | 4 | 0.0277 | 3.99x |
| damma.pgm | blur | 12 | 0.0266 | 4.15x |
| damma.pgm | sobel | 1 | 0.1431 | 1.06x |
| damma.pgm | sobel | 2 | 0.0766 | 1.99x |
| damma.pgm | sobel | 4 | 0.0368 | 4.14x |
| damma.pgm | sobel | 12 | 0.0305 | 4.99x |
| damma.ppm | blur | 1 | 0.3172 | 1.05x |
| damma.ppm | blur | 2 | 0.1675 | 1.99x |
| damma.ppm | blur | 4 | 0.0809 | 4.13x |
| damma.ppm | blur | 12 | 0.0778 | 4.29x |
| damma.ppm | sobel | 1 | 0.4255 | 1.15x |
| damma.ppm | sobel | 2 | 0.2249 | 2.18x |
| damma.ppm | sobel | 4 | 0.1210 | 4.05x |
| damma.ppm | sobel | 12 | 0.0648 | 7.56x |
| sulfur.pgm | blur | 1 | 0.0670 | 1.13x |
| sulfur.pgm | blur | 2 | 0.0364 | 2.07x |
| sulfur.pgm | blur | 4 | 0.0192 | 3.92x |
| sulfur.pgm | blur | 12 | 0.0137 | 5.49x |
| sulfur.pgm | sobel | 1 | 0.0917 | 1.08x |
| sulfur.pgm | sobel | 2 | 0.0471 | 2.10x |
| sulfur.pgm | sobel | 4 | 0.0239 | 4.15x |
| sulfur.pgm | sobel | 12 | 0.0161 | 6.15x |
| sulfur.ppm | blur | 1 | 0.2000 | 1.08x |
| sulfur.ppm | blur | 2 | 0.1039 | 2.08x |
| sulfur.ppm | blur | 4 | 0.0550 | 3.93x |
| sulfur.ppm | blur | 12 | 0.0484 | 4.46x |
| sulfur.ppm | sobel | 1 | 0.2801 | 1.03x |
| sulfur.ppm | sobel | 2 | 0.1404 | 2.06x |
| sulfur.ppm | sobel | 4 | 0.0728 | 3.96x |
| sulfur.ppm | sobel | 12 | 0.0416 | 6.94x |

Observaciones:
- **1 hilo ≈ secuencial** (speedup ~1.0x-1.15x): confirma que la
  implementación de OpenMP no agrega overhead significativo cuando no hay
  paralelismo real.
- **2 y 4 hilos escalan casi linealmente** (speedup ≈ número de hilos):
  la máquina tiene 6 núcleos físicos, así que hasta 4 hilos hay núcleo
  dedicado de sobra para cada uno.
- **12 hilos sigue mejorando pero con rendimientos decrecientes** respecto
  a lo lineal (12x ideal): el speedup real va de ~4.2x a ~7.6x según el
  caso. La máquina solo tiene 6 núcleos físicos (12 "lógicos" vía SMT/
  Hyper-Threading); los hilos 7-12 comparten núcleo físico con otro hilo
  (ejecución simultánea de 2 hilos por núcleo, no 2 núcleos reales), así
  que no se gana otro 2x completo, pero sigue habiendo beneficio porque
  SMT aprovecha ciclos ociosos.
- El `total_real_s` (lectura+filtrado+escritura) mejora menos que el
  filtrado puro porque lectura y escritura **no están paralelizadas** (se
  quedan igual de lentas que en el diseño 2); con muchos hilos, el
  filtrado deja de ser el cuello de botella y empieza a dominar la lectura
  de texto plano — consistente con la Ley de Amdahl ya señalada en la
  Fase 2.

## 6. Pruebas realizadas

### 6.1 Compilación
```bash
make clean && make
# g++ ... -Wall -Wextra ... (sin warnings) para processor, filterer,
# th_filterer (con -pthread) y omp_filterer (con -fopenmp)
```
El `Makefile` agrega reglas explícitas para `src/EjecutorPthreads.o`
(compilado con `-pthread`) y `src/EjecutorOpenMP.o` (compilado con
`-fopenmp`); el resto de objetos se comparte sin esos flags.

### 6.2 `cmp` byte a byte
Ver sección 4: 32 + 32 comparaciones, 0 fallas.

### 6.3 Valgrind — memcheck
```
$ valgrind --leak-check=full ./th_filterer images/lena.pgm out.pgm --f blur
==67469== HEAP SUMMARY:
==67469==     in use at exit: 0 bytes in 0 blocks
==67469==   total heap usage: 15 allocs, 15 frees, 2,185,400 bytes allocated
==67469== All heap blocks were freed -- no leaks are possible
==67469== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
`th_filterer` queda completamente limpio (0 fugas, 0 errores).

```
$ valgrind --leak-check=full --show-leak-kinds=all ./omp_filterer images/lena.ppm out.ppm --hilos 4
==67500== LEAK SUMMARY:
==67500==    definitely lost: 0 bytes in 0 blocks
==67500==    indirectly lost: 0 bytes in 0 blocks
==67500==      possibly lost: 960 bytes in 3 blocks
==67500==    still reachable: 2,832 bytes in 6 blocks
```
`omp_filterer` reporta **0 bytes "definitely lost"** (nuestro código no
pierde memoria); los bloques "possibly lost"/"still reachable" se rastrearon
con `--show-leak-kinds=all` y **todos** provienen de `libgomp` (el runtime
de OpenMP de GCC): `gomp_malloc`, `gomp_team_start`, `gomp_get_thread_pool`,
`omp_set_num_threads` → `gomp_new_icv`. Es el pool de hilos que `libgomp`
mantiene vivo durante toda la vida del proceso (para no tener que crear/
destruir hilos en cada `parallel for`) y libera el sistema operativo al
terminar el proceso; es un patrón de "falso positivo" ampliamente
documentado de valgrind con libgomp, no una fuga introducida por este
código — y se confirma mirando la traza: ninguna pasa por `Imagen`,
`ImagenIO`, `Filtro` ni `EjecutorOpenMP`, solo por símbolos internos
`gomp_*`.

### 6.4 Valgrind — helgrind (condiciones de carrera)
Ver sección 2: 0 errores sobre `th_filterer`.

## 7. Reglas obligatorias respetadas

- 4 cuadrantes con pthreads, uno por hilo (`th_filterer`).
- `parallel for` sobre filas con OpenMP, hilos configurables por argumento
  (`--hilos N`) o `OMP_NUM_THREADS` (`omp_filterer`).
- Sin `--f`, ambos aplican los tres filtros obligatorios y generan
  archivos separados (`_blur`/`_laplace`/`_sharpen`), igual que `filterer`.
- Se reutiliza `Filtro::aplicar` tal cual; ninguna estrategia reimplementa
  la convolución.
- Dimensiones impares manejadas correctamente (división entera, sin huecos
  ni solapes).
- Salidas verificadas idénticas (`cmp`) a la versión secuencial.
- Compilación limpia con `-Wall -Wextra`.
- Probado con damma y sulfur (PGM y PPM), como pide `CLAUDE.md` para este
  diseño.

## 8. Pendientes / dudas

- El Makefile todavía no tiene una regla explícita de dependencia de
  headers (si se cambia un `.h`, hay que `make clean` para forzar
  recompilación); no ha causado problemas porque este proyecto es pequeño,
  pero se deja anotado por si se vuelve molesto en el diseño 4.
- No se corrió el speedup para `laplace`/`sharpen` en todos los conteos de
  hilos de OpenMP (solo blur y sobel) para no multiplicar excesivamente el
  número de corridas; dado que los cuatro filtros comparten la misma
  estructura de bucle (`convolucionar`/`Sobel` con el mismo patrón de
  acceso), el comportamiento de speedup observado en blur/sobel es
  representativo (se puede ver también en la tabla de la sección 5.1,
  donde sí están los 4 filtros con pthreads a 4 hilos fijos).
- Pendiente de Fase 4: `mpi_filterer`, repartiendo franjas de filas con
  filas fantasma (halo de 1) entre procesos en contenedores Docker,
  reutilizando una vez más `Filtro::aplicar` sin cambios.
