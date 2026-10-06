# Fase 2 — Diseño 2: Versión secuencial de filtrado (`filterer`)

Fecha: 2026-10-05
Rama: `diseno-2` (parte de `diseno-1`)

## 1. Qué se hizo

Se añadió la jerarquía de filtros (`Filtro`, `FiltroBlur`, `FiltroLaplace`,
`FiltroSharpen`, `FiltroSobel`, `FiltroFactory`) y el cronómetro
(`Temporizador`), y se reescribió `src/filterer.cpp` para usarlos junto con
`Imagen`/`ImagenIO` del diseño 1. El ejecutable `filterer` admite:

```bash
./filterer entrada salida --f blur      # un solo filtro
./filterer entrada salida               # blur + laplace + sharpen (obligatorios)
```

Sin `--f` se generan `<salida>_blur`, `<salida>_laplace` y `<salida>_sharpen`
(conservando la extensión de `salida`), construidos insertando el sufijo
antes de la extensión con `strrchr`/`snprintf` (sin `std::string`).

Se extendió `Imagen` (de la rama `diseno-1`) con un método adicional:

```cpp
virtual Imagen *crearVacia(int ancho, int alto, int valorMax) const = 0;
```

implementado en `PGMImagen`/`PPMImagen`. Es el patrón "prototipo": permite
que `filterer.cpp` cree la imagen de salida del tipo correcto
(`entrada.crearVacia(...)`) sin preguntar `getCanales()` ni conocer las
subclases concretas — necesario porque ahora el código que usa `Imagen` es
genérico (no sabe si está filtrando un PGM o un PPM). Es un cambio aditivo:
no se tocó ninguna firma existente, así que `processor` (diseño 1) sigue
compilando y pasando sus pruebas sin cambios (ver sección 7).

## 2. Arquitectura de los filtros

```
include/
  Filtro.h          -> clase abstracta: aplicar(), nombre(), helpers protegidos
  FiltroBlur.h / FiltroLaplace.h / FiltroSharpen.h / FiltroSobel.h
  FiltroFactory.h   -> crea un Filtro* a partir de su nombre (string)
  Temporizador.h    -> cronómetro de tiempo real + tiempo de CPU
src/
  Filtro.cpp         -> clampear() y convolucionar() (convolución genérica 1 kernel)
  FiltroBlur.cpp / FiltroLaplace.cpp / FiltroSharpen.cpp  -> cada uno define su kernel 3x3
  FiltroSobel.cpp    -> caso especial: combina dos kernels (Gx, Gy)
  FiltroFactory.cpp
  Temporizador.cpp
  filterer.cpp       -> CLI, orquesta lectura/filtrado/escritura/medición
```

`Filtro::aplicar(entrada, salida, y0, y1, x0, x1)` filtra la región
semiabierta `[y0,y1) x [x0,x1)` de `salida`, pero puede **leer** cualquier
posición de `entrada` (no solo la región) para resolver los vecinos de
borde. `entrada` es de solo lectura (`const Imagen&`) y cada filtro escribe
únicamente dentro de la región indicada de `salida`. En este diseño
siempre se invoca con la imagen completa (`y0=0,y1=alto,x0=0,x1=ancho`),
pero la firma ya queda lista para los diseños 3 y 4: varios hilos/procesos
podrán filtrar regiones distintas de la misma imagen de entrada sin
condición de carrera, porque cada uno solo escribe su propia porción de
`salida` — se deja esta justificación documentada aquí porque es el motivo
de diseño de la firma `apply`/`aplicar` pedida por `CLAUDE.md`.

**Cada canal se filtra por separado**: tanto `Filtro::convolucionar()`
(usada por blur/laplace/sharpen) como `FiltroSobel::aplicar()` iteran
`for (int c = 0; c < canales; c++)` como bucle más interno antes de mirar
los vecinos, y el vecino se obtiene con `entrada.obtenerValor(nx, ny, c)`
— siempre fijando el canal `c`, nunca mezclando R, G y B de píxeles
distintos. Esto es exactamente lo que evita el error 3.3 documentado en
`docs/reportes/FASE_1.md` (el blur original de `filterer.cpp` trataba
R,G,B intercalados como un único canal).

## 3. Kernels usados

Los cuatro kernels son 3x3, en coordenadas `[fila][columna]` con el origen
(0,0) en la esquina superior izquierda del kernel:

**Blur (suavizado, promedio)** — pesos suman 1:
```
1/9  1/9  1/9
1/9  1/9  1/9
1/9  1/9  1/9
```

**Laplace (detección de bordes, laplaciano de 4 vecinos)** — pesos suman 0:
```
 0  -1   0
-1   4  -1
 0  -1   0
```

**Sharpen (realce)** — pesos suman 1:
```
 0  -1   0
-1   5  -1
 0  -1   0
```

**Sobel (extra, detección de bordes direccional)** — dos kernels, magnitud
`sqrt(Gx² + Gy²)`:
```
Gx:              Gy:
-1   0   1       -1  -2  -1
-2   0   2        0   0   0
-1   0   1        1   2   1
```

## 4. Política de bordes

Se eligió **replicación de borde** (border replication / clamp de
coordenadas): para un vecino `(x+kx, y+ky)` que cae fuera de la imagen,
se usa el píxel válido más cercano, recortando la coordenada al rango
`[0, ancho-1]` / `[0, alto-1]` (`Filtro::clampear`, usado tanto en
`convolucionar()` como en `FiltroSobel::aplicar()`). **No** se usa
renormalización por pesos válidos.

Se prefirió replicación porque:
- Es más simple y predecible: el kernel siempre ve 9 valores reales
  (nunca hay que recalcular una suma de pesos distinta por posición), lo
  que además es más fácil de paralelizar correctamente en los diseños 3/4
  (no hay casos especiales en los bordes de una región, solo en los
  bordes de la imagen completa).
- Para blur/sharpen evita el oscurecimiento artificial que a veces produce
  rellenar con 0 en vez de repetir el borde.

Es una decisión de diseño documentada, no la única válida; la alternativa
(normalizar dividiendo solo por la suma de los pesos de los vecinos que sí
existen) también es correcta y se podría sustituir cambiando únicamente
`Filtro::convolucionar()` sin tocar los filtros concretos.

## 5. Laplace y los valores negativos

El kernel de Laplace suma 0, así que `suma` en `convolucionar()` puede ser
negativa (bordes) o positiva (zonas de alto contraste), y su magnitud puede
superar `valorMax`. Se trata con **valor absoluto antes del clamp final**:
`FiltroLaplace` pasa `valorAbsoluto=true` a `convolucionar()`, que hace
`suma = std::fabs(suma)` y luego recorta a `[0, valorMax]`.

Se eligió valor absoluto (en vez de, por ejemplo, desplazar el rango sumando
un offset como `valorMax/2`, o simplemente recortar negativos a 0) porque:
- Es el tratamiento estándar para visualizar bordes con Laplace: interesa
  la **magnitud** del cambio de intensidad, no su signo: un borde de claro
  a oscuro y uno de oscuro a claro deben verse igual de "brillantes".
- Recortar negativos a 0 sin valor absoluto perdería la mitad de los
  bordes detectados (los que dan signo negativo).
- Puede verse en `output/png/*_laplace*.png`: el resultado son líneas
  brillantes sobre fondo oscuro, sin zonas que se pierdan por quedar en 0.

## 6. Mediciones (tiempo de CPU y tiempo real)

`Temporizador` usa `std::chrono::steady_clock` para tiempo real y `clock()`
para tiempo de CPU, con una instancia nueva por fase (lectura, filtrado,
escritura), siguiendo exactamente lo pedido en `CLAUDE.md`. `filterer`
imprime una cabecera CSV y luego **una línea CSV por filtro aplicado** (si
no se da `--f`, son 3 líneas: blur, laplace, sharpen; la fase de lectura se
mide una sola vez y se repite en cada línea porque el archivo se lee una
sola vez para los tres filtros).

Formato de cada línea:
```
entrada,salida,filtro,ancho,alto,lectura_real_s,lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,escritura_real_s,escritura_cpu_s,total_real_s,total_cpu_s
```

### Tabla de tiempos reales (ejecución real en esta máquina, `make clean && make` sin valgrind)

Comando usado para cada fila: `./filterer images/<img>.<ext> output/<img>_<filtro>.<ext> --f <filtro>`

| entrada | filtro | ancho x alto | lectura (s) | filtrado (s) | escritura (s) | total real (s) | total CPU (s) |
|---|---|---|---:|---:|---:|---:|---:|
| lena.pgm | blur | 512x512 | 0.0169 | 0.0228 | 0.0107 | 0.0503 | 0.0501 |
| lena.pgm | laplace | 512x512 | 0.0171 | 0.0254 | 0.0112 | 0.0537 | 0.0535 |
| lena.pgm | sharpen | 512x512 | 0.0169 | 0.0230 | 0.0111 | 0.0510 | 0.0508 |
| lena.pgm | sobel | 512x512 | 0.0168 | 0.0305 | 0.0110 | 0.0583 | 0.0580 |
| lena.ppm | blur | 128x128 | 0.0033 | 0.0042 | 0.0020 | 0.0095 | 0.0095 |
| lena.ppm | laplace | 128x128 | 0.0033 | 0.0045 | 0.0021 | 0.0099 | 0.0098 |
| lena.ppm | sharpen | 128x128 | 0.0031 | 0.0044 | 0.0027 | 0.0101 | 0.0101 |
| lena.ppm | sobel | 128x128 | 0.0033 | 0.0059 | 0.0020 | 0.0111 | 0.0111 |
| fruit.pgm | blur | 900x450 | 0.0260 | 0.0347 | 0.0162 | 0.0770 | 0.0767 |
| fruit.pgm | laplace | 900x450 | 0.0248 | 0.0375 | 0.0155 | 0.0777 | 0.0774 |
| fruit.pgm | sharpen | 900x450 | 0.0245 | 0.0349 | 0.0163 | 0.0757 | 0.0755 |
| fruit.pgm | sobel | 900x450 | 0.0269 | 0.0487 | 0.0163 | 0.0919 | 0.0915 |
| fruit.ppm | blur | 900x450 | 0.0777 | 0.1030 | 0.0480 | 0.2288 | 0.2279 |
| fruit.ppm | laplace | 900x450 | 0.0785 | 0.1142 | 0.0455 | 0.2382 | 0.2362 |
| fruit.ppm | sharpen | 900x450 | 0.0760 | 0.1044 | 0.0490 | 0.2294 | 0.2285 |
| fruit.ppm | sobel | 900x450 | 0.0792 | 0.1402 | 0.0463 | 0.2656 | 0.2647 |
| puj.pgm | blur | 1920x600 | 0.0735 | 0.1005 | 0.0464 | 0.2204 | 0.2185 |
| puj.pgm | laplace | 1920x600 | 0.0691 | 0.1069 | 0.0459 | 0.2219 | 0.2211 |
| puj.pgm | sharpen | 1920x600 | 0.0752 | 0.1008 | 0.0498 | 0.2257 | 0.2249 |
| puj.pgm | sobel | 1920x600 | 0.0748 | 0.1372 | 0.0469 | 0.2588 | 0.2574 |
| puj.ppm | blur | 1920x600 | 0.2147 | 0.2942 | 0.1393 | 0.6482 | 0.6460 |
| puj.ppm | laplace | 1920x600 | 0.2270 | 0.3190 | 0.1356 | 0.6817 | 0.6794 |
| puj.ppm | sharpen | 1920x600 | 0.2094 | 0.2999 | 0.1396 | 0.6489 | 0.6467 |
| puj.ppm | sobel | 1920x600 | 0.2254 | 0.4029 | 0.1442 | 0.7725 | 0.7696 |

Observaciones (consistentes con lo que anticipa `CLAUDE.md`):
- **Lectura/escritura dominan o compiten con el filtrado**: al ser PGM/PPM
  texto plano, convertir cada valor con `fscanf("%d")`/`fprintf("%d")` tiene
  un costo comparable (a veces mayor) al de la convolución 3x3 en sí. Por
  ejemplo en `puj.ppm` blur: lectura 0.215s + escritura 0.139s = 0.354s
  frente a 0.294s de filtrado puro.
- **PPM tarda ~3x más que PGM** en la misma imagen y mismo filtro (p. ej.
  `fruit.pgm` blur: 0.077s vs `fruit.ppm` blur: 0.229s) porque hay 3 veces
  más valores de píxel que leer/escribir/filtrar (un canal por color).
- **Sobel es el más lento en filtrado** porque convoluciona con *dos*
  kernels (Gx y Gy) en vez de uno solo, prácticamente el doble de
  multiplicaciones por píxel que blur/sharpen.
- `total_cpu_s` ≈ `total_real_s` en todas las filas, como se espera de un
  programa **secuencial de un solo hilo**: el tiempo de CPU no puede
  superar al tiempo real porque solo hay un núcleo trabajando a la vez
  (en los diseños 3/4, con varios hilos, sí podrá superarlo).

Reproducir:
```bash
make clean && make
mkdir -p output
for img in lena fruit puj; do
  for ext in pgm ppm; do
    for filtro in blur laplace sharpen sobel; do
      ./filterer images/${img}.${ext} output/${img}_${filtro}.${ext} --f ${filtro}
    done
  done
done
```

## 7. Pruebas realizadas

### 7.1 Compilación
```bash
make clean && make
# g++ -Wall -Wextra -std=c++17 -Iinclude ...   (sin warnings, processor y filterer)
```

### 7.2 Regresión del diseño 1
`Imagen` se extendió con `crearVacia()`; se reprocesaron las 14 imágenes de
`images/` con `processor` y se verificaron por valores numéricos (mismo
script de la Fase 1): **0 fallas**, confirma que la extensión no rompió el
diseño 1.

### 7.3 Filtrado de lena, fruit, puj (PGM y PPM) con los 4 filtros
24 combinaciones (3 imágenes x 2 formatos x 4 filtros) ejecutadas con
`--f <filtro>`; todas terminaron con código de salida 0 y generaron su
archivo de salida (tiempos en la tabla de la sección 6).

### 7.4 Conversión a PNG e inspección visual
```bash
for f in output/*.pgm output/*.ppm; do
  magick "$f" "output/png/$(basename "${f%.*}")_${f##*.}.png"
done
```
Se revisaron visualmente (24 PNG en `output/png/`, no versionados —
`output/` está en `.gitignore`):
- **Blur**: suaviza correctamente, sin artefactos de color (los 3 canales
  de PPM se ven coherentes, no hay "manchas" de color producidas por mezclar
  canales).
- **Sharpen**: realza bordes manteniendo los colores originales reconocibles.
- **Laplace** y **Sobel**: producen el clásico resultado de "líneas
  brillantes sobre fondo oscuro" marcando los contornos (sombrero, cabello
  y rasgos de `lena`; edificios y vegetación en `puj`), sin zonas vacías
  por pérdida de bordes de signo negativo (ver sección 5).

### 7.5 Valgrind
```bash
$ valgrind --leak-check=full --error-exitcode=1 ./filterer images/lena.pgm out.pgm --f blur
==57089== HEAP SUMMARY:
==57089==     in use at exit: 0 bytes in 0 blocks
==57089==   total heap usage: 11 allocs, 11 frees, 2,184,184 bytes allocated
==57089== All heap blocks were freed -- no leaks are possible
==57089== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

$ valgrind --leak-check=full --error-exitcode=1 ./filterer images/lena.ppm out.ppm   # sin --f, 3 filtros
==57108== HEAP SUMMARY:
==57108==     in use at exit: 0 bytes in 0 blocks
==57108==   total heap usage: 21 allocs, 21 frees, 882,680 bytes allocated
==57108== All heap blocks were freed -- no leaks are possible
==57108== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
0 errores, 0 fugas en ambos casos (un filtro único y los tres filtros por
defecto). Los tiempos que imprime el programa bajo valgrind son mucho más
altos que en ejecución normal (instrumentación de cada acceso a memoria);
por eso la tabla de tiempos de la sección 6 usa ejecuciones **sin**
valgrind.

## 8. Cómo agregar un filtro nuevo

El Sobel de este mismo diseño es el ejemplo: es el único de los cuatro que
**no** se agregó reutilizando `Filtro::convolucionar()` (porque necesita
combinar dos kernels, Gx y Gy, no uno solo) y aun así tomó solo dos
archivos nuevos y una línea en la fábrica. Pasos genéricos:

1. **Crear el header** `include/FiltroX.h`, heredando de `Filtro`:
   ```cpp
   class FiltroX : public Filtro {
   public:
       void aplicar(const Imagen &entrada, Imagen &salida,
                    int y0, int y1, int x0, int x1) const override;
       const char *nombre() const override;
   };
   ```
2. **Implementar** `src/FiltroX.cpp`:
   - Si el filtro usa un único kernel 3x3 (como blur/laplace/sharpen),
     basta definir el kernel `static const float kernel[3][3]` y llamar a
     `convolucionar(entrada, salida, y0, y1, x0, x1, kernel, valorAbsoluto)`
     — un filtro nuevo de este tipo son literalmente 3 líneas de código
     (ver `FiltroBlur.cpp`/`FiltroSharpen.cpp`).
   - Si necesita algo distinto de "un kernel 3x3 con clamp" (como Sobel,
     que combina dos kernels y calcula una magnitud), se escribe
     `aplicar()` completo; se puede seguir usando `Filtro::clampear()`
     (protegido, heredado) para la política de bordes y el recorte final,
     como hace `FiltroSobel.cpp`.
3. **Registrar el nombre** en `FiltroFactory::crear()` (una línea):
   ```cpp
   if (std::strcmp(nombre, "x") == 0) return new FiltroX();
   ```
4. **Agregar los dos archivos al Makefile** (`FILTROS_SRCS`).

No hace falta tocar `Imagen`, `ImagenIO`, `Temporizador` ni `filterer.cpp`:
la CLI (`--f x`), la medición de tiempos y el manejo de PGM/PPM ya
funcionan para cualquier filtro que pase por `FiltroFactory`. Eso es lo que
pedía la consigna al recomendar investigar Sobel "para demostrar
extensibilidad": el costo real de agregarlo fueron 2 archivos de ~40 líneas
y una línea en la fábrica, sin modificar ningún archivo existente salvo el
Makefile.

## 9. Reglas obligatorias respetadas

- OO, arreglos dinámicos / `char*` (ningún `std::vector`/`std::string` en
  el código nuevo).
- Cada canal se filtra por separado (sección 2).
- Kernel 3x3 y `apply`/`aplicar(entrada, salida, y0,y1,x0,x1)` que filtra
  una región, reutilizable en los diseños 3/4 sin cambios de firma.
- `FilterFactory`/`FiltroFactory` crea el filtro por nombre.
- `Timer`/`Temporizador` mide tiempo de CPU y tiempo real.
- Compilación limpia con `-Wall -Wextra`.
- Medición de lectura, filtrado, escritura y total, por separado, impresa
  en CSV (una línea por ejecución/filtro).

## 10. Pendientes / dudas

- No se comparó aún contra una referencia externa (p. ej. el repo
  `netpbm_filters` sugerido en la consigna); los resultados se validaron
  por inspección visual (PNG) y por coherencia de los valores (clamp,
  ausencia de mezcla de canales). Si se requiere una comparación más
  rigurosa se puede agregar en una fase posterior.
- El error 3.3 de `FASE_1.md` (blur mezclando canales en el código
  original) queda formalmente corregido y explicado aquí: la causa raíz
  era iterar por índice de píxel en vez de por índice de valor/canal; la
  arquitectura de `Imagen` (acceso siempre vía `obtenerValor(x,y,canal)`)
  lo previene de raíz en todos los filtros, no solo en blur.
- Pendiente de Fase 3: dividir la imagen en 4 cuadrantes con pthreads
  (`th_filterer`) y en filas con OpenMP (`omp_filterer`), reutilizando
  `Filtro::aplicar(entrada, salida, y0,y1,x0,x1)` tal cual está.
