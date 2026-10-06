# Fase 1 — Diseño 1: Aplicación base (`processor`)

Fecha: 2026-10-05
Rama: `diseno-1` (parte de `main`)

## 1. Qué se hizo

Se reemplazó `src/processor.cpp` (versión original del profesor, con los
errores descritos abajo) por una implementación orientada a objetos,
organizada en `include/` + `src/`, con un `Makefile` en la raíz. El
ejecutable resultante se sigue llamando `processor` y conserva la misma
interfaz de uso (`./processor entrada salida`), agregando soporte de lectura
desde `stdin` con `-` como entrada.

## 2. Arquitectura

```
include/
  Imagen.h       -> clase abstracta base
  PGMImagen.h     -> imagen de 1 canal (P2)
  PPMImagen.h     -> imagen de 3 canales (P3)
  ImagenIO.h      -> fábrica de lectura/escritura (detecta P2/P3)
src/
  Imagen.cpp
  PGMImagen.cpp
  PPMImagen.cpp
  ImagenIO.cpp
  processor.cpp   -> main del diseño 1
  filterer.cpp    -> sin tocar, es del diseño 2 (próxima fase)
```

- **`Imagen`** (abstracta): guarda `ancho`, `alto`, `valorMax`, `canales` y un
  único arreglo dinámico `int* pixeles`, **intercalado por canal dentro de
  cada fila**: `indice = (y*ancho + x)*canales + canal`. Esto significa que en
  una PPM los 3 valores R,G,B de un mismo píxel quedan contiguos. Se eligió
  esta representación (en vez de un arreglo por canal) porque es la más
  cercana al formato de archivo (simplifica lectura/escritura secuencial) y,
  como cada acceso pasa por `obtenerValor(x,y,canal)` / `asignarValor(...)`,
  los filtros del diseño 2 podrán tratar cada canal por separado sin
  importarles el layout interno — solo usan el índice de canal. El destructor
  libera el arreglo (`delete[]`), y el constructor/operador de copia están
  eliminados (`= delete`) porque cada imagen es dueña única de su arreglo de
  píxeles (evita doble `delete` por copias accidentales).
- **`PGMImagen`** / **`PPMImagen`**: fijan `canales=1` o `canales=3` en el
  constructor y responden `numeroMagico()` ("P2"/"P3") y `nombreFormato()`
  ("PGM"/"PPM"), métodos virtuales puros de `Imagen`.
- **`ImagenIO`**: clase con métodos estáticos `leer(ruta)` y
  `escribir(imagen, ruta)`, es decir, cumple el rol de fábrica que menciona
  `CLAUDE.md`. `leer()`:
  1. Abre el archivo (o usa `stdin` si `ruta == "-"`).
  2. Lee el número mágico, ancho, alto y valor máximo con funciones
     auxiliares (`leerMagico`, `leerEntero`) que primero llaman a
     `saltarEspaciosYComentarios()`.
  3. Según el número mágico construye una `PGMImagen` o `PPMImagen` (si no es
     ni P2 ni P3, falla con mensaje de error).
  4. Lee los `ancho*alto*canales` valores de píxel, cada uno también
     precedido por `saltarEspaciosYComentarios()`, así que un comentario
     puede aparecer en el encabezado o intercalado entre los valores de
     píxel y se ignora igual.
  - `saltarEspaciosYComentarios()` recorre carácter por carácter: si
    encuentra `#` descarta hasta el siguiente `\n`; si encuentra un
    carácter que no es espacio, lo devuelve al buffer con `ungetc` y
    termina. Así se cumple la regla "ignorar comentarios en cualquier parte
    del encabezado" (y, de paso, en cualquier parte del archivo, que es el
    comportamiento real del formato plain PGM/PPM).

## 3. Errores del código original (`CLAUDE.md`) y cómo se resolvieron

El código original (ver commit base en `main`, antes de esta rama) tenía los
siguientes problemas:

### 3.1 `malloc(pixel_count)` reserva bytes, no enteros
```cpp
pixels = (int *) malloc(pixel_count);
```
`pixel_count` es un conteo de **elementos**, pero `malloc` recibe **bytes**.
Al reservar solo `pixel_count` bytes para un arreglo de `int` (4 bytes cada
uno en la mayoría de plataformas), el arreglo real necesita 4 veces ese
espacio: cualquier imagen con más de `pixel_count/4` elementos escribe fuera
del bloque reservado (heap overflow), corrompiendo memoria de forma que
puede no fallar de inmediato pero sí de forma impredecible más adelante.

**Solución**: se usa `new int[getCantidadValores()]` dentro del constructor
de `Imagen` (`src/Imagen.cpp`), donde `new[]` ya reserva
`cantidad * sizeof(int)` automáticamente — no hay forma de cometer este error
con `new[]` porque el tamaño en bytes no se pasa a mano.

### 3.2 Condición `strcmp(magic, "P3") != 0` invertida + redeclaración de `pixel_count` (shadowing)
```cpp
int pixel_count = width * height;
if (strcmp(magic, "P3") != 0){
  int pixel_count = width * height * 3;   // nueva variable local, no reemplaza la de afuera
}
```
Dos errores combinados:
- La condición está invertida: `strcmp(...) != 0` es verdadera cuando
  **NO** es P3, es decir, el bloque que multiplica por 3 se ejecuta cuando
  la imagen es PGM (1 canal) y se salta cuando es PPM (3 canales) — al
  revés de lo que se necesita.
- Aunque la condición estuviera bien, el `int pixel_count` dentro del `if`
  declara una variable **nueva**, visible solo dentro de ese bloque (shadowing
  de la de afuera). Al salir del `if` la de afuera sigue valiendo
  `width*height`, sin multiplicar por 3. El resultado neto: para una imagen
  PPM, el programa solo reserva y lee `width*height` valores (1/3 de los
  necesarios) en vez de `width*height*3`.

**Solución**: el número de canales queda fijo por el tipo concreto
(`PGMImagen` usa 1, `PPMImagen` usa 3) desde el constructor de `Imagen`, y
`getCantidadValores()` siempre calcula `ancho*alto*canales` sin condicionales
ni variables duplicadas. En `ImagenIO::leer()` la detección de formato se
hace una sola vez, comparando el número mágico leído contra `"P2"`/`"P3"`
(sin negación) para decidir qué subclase instanciar.

### 3.3 El blur trata R, G, B intercalados como si fueran un único canal
En `filterer.cpp` el kernel 3x3 recorre `pixels[idx]` con
`idx = ny*width + nx`, es decir, un índice por **píxel**, no por **canal**.
En una imagen PPM eso mezcla valores de R, G y B de píxeles vecinos como si
fueran todos del mismo canal, produciendo un resultado de color incorrecto
(además de quedar agravado por el error 3.2, que ya truncaba la lectura).

Este error pertenece al diseño 2 (filtros) y se documentará y corregirá
en `docs/reportes/FASE_2.md`; se deja anotado aquí porque está en la lista de
errores conocidos de `CLAUDE.md`. La arquitectura de `Imagen` ya construida
en esta fase lo previene de raíz: el acceso siempre es
`obtenerValor(x, y, canal)` / `asignarValor(x, y, canal, valor)`, así que un
filtro que itera `canal` de 0 a `getCanales()-1` nunca mezcla canales.

### 3.4 Otros problemas (faltan includes, validación de `argc`, memoria, archivo de salida)
- **Includes faltantes**: el original solo incluye `<iostream>` pero usa
  `strcmp`/`malloc`/`free` (necesitan `<cstring>`/`<cstdlib>`) y, en
  `filterer.cpp`, `std::max`/`std::min` (necesita `<algorithm>`). Compilaba
  solo porque algún header transitivo los arrastraba, pero no es portable ni
  correcto. **Solución**: la nueva implementación incluye explícitamente
  `<cstdio>`, `<cstring>`, `<cstdlib>`, `<cctype>` donde corresponde.
- **`argc<2` en vez de `argc<3`**: con `argc<2` el programa deja pasar una
  invocación con un solo argumento (`argv[1]` presente pero `argv[2]`
  ausente), y luego usa `argv[2]` en `fopen` leyendo memoria fuera de los
  límites de `argv` (comportamiento indefinido). **Solución**: se valida
  `argc < 3` en `processor.cpp`.
- **No libera memoria**: el original nunca llama a `free(pixels)` en el
  camino exitoso (solo en el de error de lectura). **Solución**: `Imagen`
  libera su arreglo en el destructor (RAII); en `processor.cpp` se llama
  `delete imagen;` explícitamente antes de retornar.
- **No valida apertura del archivo de salida**: `fopen(argv[2], "w")` se usa
  sin comprobar si devolvió `nullptr` (por ejemplo, ruta de salida en un
  directorio sin permisos de escritura), lo que provoca un `fprintf`/`fclose`
  sobre un puntero nulo. **Solución**: `ImagenIO::escribir()` comprueba el
  resultado de `fopen` y devuelve `false` con un mensaje de error si falla;
  `processor.cpp` retorna código de salida 1 en ese caso.
- **No maneja comentarios**: el original lee con `fscanf("%2s"/"%d", ...)`
  directamente, sin saltar `#...\n`, así que un comentario en el encabezado
  rompería el parseo (el siguiente `%d` leería el texto del comentario).
  **Solución**: ver `saltarEspaciosYComentarios()` en la sección 2.

## 4. Reglas obligatorias respetadas

- Arreglos dinámicos (`int* pixeles`), ningún `std::vector`.
- `char*`/arreglos de `char` (p. ej. `char magico[3]`), ningún `std::string`.
- Detección automática P2/P3 por número mágico (`ImagenIO::leer`).
- Comentarios `#` ignorados en cualquier parte del archivo (encabezado y
  datos de píxel), ver sección 2 y prueba en sección 5.
- La salida conserva el formato de la entrada: PGM produce "P2" en el
  encabezado de salida, PPM produce "P3" (vía `numeroMagico()` de la
  subclase concreta).
- Entrada por archivo o por `stdin` con `-` (sección 5).

## 5. Pruebas realizadas

Compilación limpia:
```bash
make clean && make
# g++ -Wall -Wextra -std=c++17 -Iinclude ...   (sin warnings)
```

### 5.1 Las 14 imágenes de `images/`

Se procesó cada imagen (`./processor images/X output/X_proc`) y se comparó
contra el original con un script de verificación por **valores numéricos**
(ignora formato de texto, espacios y comentarios; solo compara número
mágico, ancho, alto, valor máximo y cada valor de píxel uno a uno):

```
OK: images/damma.pgm == output/damma_proc.pgm (1278000 valores de pixel, P2 1000x1278 max=255)
OK: images/feep2.pgm == output/feep2_proc.pgm (168 valores de pixel, P2 24x7 max=15)
OK: images/feep.pgm == output/feep_proc.pgm (168 valores de pixel, P2 24x7 max=15)
OK: images/fruit.pgm == output/fruit_proc.pgm (405000 valores de pixel, P2 900x450 max=255)
OK: images/lena2.pgm == output/lena2_proc.pgm (262144 valores de pixel, P2 512x512 max=255)
OK: images/lena_blur.pgm == output/lena_blur_proc.pgm (262144 valores de pixel, P2 512x512 max=255)
OK: images/lena.pgm == output/lena_proc.pgm (262144 valores de pixel, P2 512x512 max=255)
OK: images/puj.pgm == output/puj_proc.pgm (1152000 valores de pixel, P2 1920x600 max=255)
OK: images/sulfur.pgm == output/sulfur_proc.pgm (823000 valores de pixel, P2 823x1000 max=255)
OK: images/damma.ppm == output/damma_proc.ppm (3834000 valores de pixel, P3 1000x1278 max=255)
OK: images/fruit.ppm == output/fruit_proc.ppm (1215000 valores de pixel, P3 900x450 max=255)
OK: images/lena.ppm == output/lena_proc.ppm (49152 valores de pixel, P3 128x128 max=255)
OK: images/puj.ppm == output/puj_proc.ppm (3456000 valores de pixel, P3 1920x600 max=255)
OK: images/sulfur.ppm == output/sulfur_proc.ppm (2469000 valores de pixel, P3 823x1000 max=255)
=== Fallas: 0 ===
```
(`feep.pgm` y `feep2.pgm` representan la misma imagen con distinto layout de
texto en el original — una fila por línea vs. un valor por línea — y ambas
se leyeron y verificaron correctamente, lo que confirma que el parser no
depende del formato de líneas.)

### 5.2 Comentarios en cualquier parte del archivo

Se construyó una copia de `feep.pgm` con comentarios insertados en el
encabezado (varias líneas seguidas), justo antes del valor máximo, al final
de una fila de píxeles, y en una línea suelta entre filas de píxeles:

```
P2
# comentario de encabezado linea 1
# comentario de encabezado linea 2
24 7
# comentario justo antes del maxval
15
0  0  0 ...
0  3  3 ... 15 15 15  0 # comentario al final de una fila de pixeles
...
# comentario suelto entre filas de pixeles
0  3  0 ...
```

Resultado:
```
Leida '.../feep_con_comentarios.pgm': PGM 24x7, 1 canal(es), max=15
OK: images/feep.pgm == output/feep_comentarios_proc.pgm (168 valores de pixel, P2 24x7 max=15)
```
Los valores resultantes son idénticos a los de `feep.pgm` sin comentarios.

### 5.3 Lectura desde `stdin` con `-`

```bash
./processor - output/lena_stdin_proc.pgm < images/lena.pgm
# OK: images/lena.pgm == output/lena_stdin_proc.pgm (262144 valores de pixel, P2 512x512 max=255)

./processor - output/lena_stdin_proc.ppm < images/lena.ppm
# OK: images/lena.ppm == output/lena_stdin_proc.ppm (49152 valores de pixel, P3 128x128 max=255)
```
Funciona tanto para PGM como para PPM.

### 5.4 Manejo de errores

```bash
$ ./processor images/lena.pgm
Uso: ./processor <entrada.pgm|entrada.ppm|-> <salida>
Use '-' como entrada para leer desde la entrada estandar (stdin).
$ echo $?
1

$ ./processor images/no_existe.pgm output/x.pgm
Error: no se pudo abrir 'images/no_existe.pgm' para lectura.
$ echo $?
1
```

### 5.5 Valgrind

```bash
$ valgrind --leak-check=full --error-exitcode=1 ./processor images/lena.pgm output/lena_valgrind.pgm
...
==53224== HEAP SUMMARY:
==53224==     in use at exit: 0 bytes in 0 blocks
==53224==   total heap usage: 8 allocs, 8 frees, 1,135,568 bytes allocated
==53224== All heap blocks were freed -- no leaks are possible
==53224== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

$ valgrind --leak-check=full --error-exitcode=1 ./processor images/lena.ppm output/lena_valgrind.ppm
...
==53360== HEAP SUMMARY:
==53360==     in use at exit: 0 bytes in 0 blocks
==53360==   total heap usage: 8 allocs, 8 frees, 283,600 bytes allocated
==53360== All heap blocks were freed -- no leaks are possible
==53360== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
Se probó tanto con PGM (`lena.pgm`) como con PPM (`lena.ppm`) para cubrir
ambas rutas de código (`PGMImagen`/`PPMImagen`); cero errores y cero fugas en
ambos casos.

## 6. Pendientes / dudas

- `src/filterer.cpp` queda sin modificar en esta rama; se reescribirá en la
  rama `diseno-2` reutilizando `Imagen`/`ImagenIO` y agregando la jerarquía
  `Filter`/`FilterFactory` descrita en `CLAUDE.md`.
- El error 3.3 (blur mezclando canales) se corregirá y documentará en la
  Fase 2, cuando se implemente el filtrado real; aquí solo se explica por
  qué la arquitectura de `Imagen` ya lo previene a nivel de diseño.
- No se definió todavía una política de bordes ni clamping (no aplica a este
  diseño, que no filtra); se abordará en la Fase 2.
