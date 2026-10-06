# Fase 0 — Preparación del entorno y entendimiento del proyecto

Fecha: 2026-10-05
Curso: Programación Paralela (300CIP013), Pontificia Universidad Javeriana Cali, 2026-II.

## 1. Resumen de la consigna (`docs/consigna.pdf`)

Micro-Proyecto No. 2 — *Manipulating images with Parallel programming*. Repositorio
de referencia del profesor para los filtros: https://github.com/japeto/netpbm_filters

Pregunta de interés del enunciado: cómo influye la programación paralela (OpenMP,
Pthreads y MPI en un entorno simulado con Docker) en el tiempo de ejecución del
filtrado de imágenes PPM y PGM frente a una implementación secuencial.

Formato de archivo PGM/PPM esperado:
- Línea 1: número mágico (`P2` para PGM, `P3` para PPM).
- Línea 2 (opcional): comentarios que comienzan con `#`.
- Línea 3: ancho y alto separados por espacio.
- Línea 4: valor máximo de color (usualmente 255).
- Líneas siguientes: valores de los píxeles (1 valor por píxel en P2, 3 en P3).

Carga automática, no interactiva; debe soportar lectura desde stdin.

### Diseños pedidos
1. **Diseño 1 — Aplicación base (`processor`)**: lee y reescribe la imagen, OO,
   extensible. `./processor lena.ppm lena2.ppm`
2. **Diseño 2 — Versión secuencial (`filterer`)**: filtros blur, laplace y
   sharpen (realce). Probar con lena, fruit, puj. Medir tiempo de CPU y de
   ejecución total. `./filterer fruit.ppm fruit_blur.ppm --f blur`
3. **Diseño 3 — Memoria compartida**:
   - `th_filterer` (Pthreads): imagen dividida en 4 regiones (cuadrantes:
     arriba-izq, arriba-der, abajo-izq, abajo-der), un hilo por región. Probar
     con damma y sulfur.
   - `omp_filterer` (OpenMP): filtros aplicados en paralelo sobre la imagen de
     entrada. Probar con damma y sulfur.
   - Medir tiempo de CPU y de ejecución total en ambos.
4. **Diseño 4 — Memoria distribuida (`mpi_filterer`)**: paso de mensajes entre
   contenedores Docker con MPI; los nodos intercambian imágenes o regiones y
   aplican los tres filtros. Probar con al menos dos imágenes de entrada. Medir
   tiempo de CPU y de ejecución por nodo.

### Recomendaciones del enunciado
- Código modular, orientado a objetos.
- Arreglos en lugar de `vector`, `char*` en lugar de `string`.
- Investigar sobre el filtro Sobel (extra, demuestra extensibilidad).

### Entregables
- Análisis de los 4 diseños respondiendo: (1) por qué aprovechan o no múltiples
  núcleos, (2) qué estructuras de datos se usaron y si se pueden mejorar, (3) qué
  tan fácil es agregar nuevos filtros, (4) impacto del tamaño de la imagen en
  eficiencia/desempeño.
- Informe en PDF (único formato aceptado, entrega por el aula digital, sin
  correo) con enlaces al código y al video (≤15 min, todos los integrantes
  participan).
- Cada diseño implementado en su propia rama del repositorio.

### Rúbrica (100 pts)
| Criterio | Pts |
|---|---|
| Implementación de cada diseño | 30 |
| Análisis de ventajas/desventajas de cada diseño | 20 |
| Respuestas + comparación de rendimiento/complejidad/mantenibilidad | 20 |
| Justificación de decisiones, estructuras de datos, funciones | 10 |
| Presentación de criterios en informe + descripción en video | 20 |

Todo esto es consistente con las reglas obligatorias y la arquitectura propuesta
en `CLAUDE.md` (que agrega el detalle técnico: jerarquía `Image`/`PGMImage`/
`PPMImage`, `Filter` con `apply()` por región, `FilterFactory`, `Timer`, halos
de 1 fila para MPI, etc.).

## 2. Código base del profesor (`src/`)

- `src/processor.cpp` (diseño 1) y `src/filterer.cpp` (diseño 2), ambos con
  `.md` de compilación/ejecución muy breves (el de `processor.md` indica
  explícitamente "Esta incompleto").
- Errores conocidos ya identificados en `CLAUDE.md` (se documentarán con detalle,
  línea por línea, en el reporte de la Fase 1 cuando se audite el código):
  - `malloc(pixel_count)` reserva bytes en vez de enteros → desborda memoria.
  - Condición `strcmp(magic, "P3") != 0` invertida y `pixel_count` redeclarado
    (shadowing) dentro del `if` → en PPM solo se lee 1/3 de los valores.
  - El blur trata R, G, B intercalados como si fueran un único canal → color
    incorrecto.
  - Faltan `<cstring>` y `<cstdlib>`; no se manejan comentarios `#`; valida
    `argc<2` en vez de `argc<3`; no libera memoria; no valida la apertura del
    archivo de salida.

## 3. Verificación del entorno

Máquina: Fedora Linux (ver `uname`), shell bash, usuario `jdavidruanob`.

### Estado final (tras instalar los paquetes faltantes)

| Herramienta | Estado | Versión / detalle |
|---|---|---|
| `git` | ✅ instalado | 2.55.0 |
| `make` | ✅ instalado | GNU Make 4.4.1 |
| `gcc` (C) | ✅ instalado | 15.3.1 (Red Hat 15.3.1-1) |
| `g++` (C++) | ✅ instalado | 15.3.1 (Red Hat 15.3.1-1) |
| OpenMP (`-fopenmp`) | ✅ **probado** | compilado y ejecutado un "hola mundo" con `#pragma omp parallel` (`-Wall -Wextra -fopenmp`), 4 hilos (`OMP_NUM_THREADS=4`), salida correcta con los 4 hilos reportándose |
| `valgrind` | ✅ instalado | 3.27.1 |
| ImageMagick (`magick`) | ✅ instalado | 7.1.2-31 Q16-HDRI |
| `docker` | ✅ instalado | 29.6.2, build 1.fc43 |
| `docker compose` (plugin v2) | ✅ instalado | 5.5.1 |
| Servicio `docker` | ✅ activo | `systemctl is-active docker` → `active` |
| Grupo `docker` | ✅ ya pertenece | `id` confirma `groups=...,969(docker)` — no hizo falta `usermod` |
| MPI (`mpic++` / `mpirun`) | ✅ instalado | Open MPI 5.0.8, binarios en `/usr/lib64/openmpi/bin/`. No quedan en el `PATH` por defecto: hay que cargar el módulo (ver nota abajo) |

### Nota sobre MPI y `environment-modules`

En Fedora, `mpic++` y `mpirun` de OpenMPI no se agregan al `PATH` automáticamente.
Para usarlos en una sesión de shell:

```bash
source /etc/profile.d/modules.sh   # solo si el shell no es de login y `module` no existe aún
module load mpi/openmpi-x86_64
```

Se verificó así: `which mpic++ mpirun` → `/usr/lib64/openmpi/bin/{mpic++,mpirun}`,
`mpirun --version` → `Open MPI 5.0.8`. Esto habrá que tenerlo en cuenta al escribir
el `Makefile` del diseño 4 (o documentarlo en el reporte de esa fase) porque
cualquier terminal nueva necesita cargar el módulo antes de compilar/ejecutar
`mpi_filterer`.

### Comandos `dnf` pendientes (ejecutar manualmente, con `sudo`)

```bash
# Compilador C++ (incluye libgomp para OpenMP)
sudo dnf install -y gcc-c++

# Valgrind, para el diseño 2 (requisito de verificación)
sudo dnf install -y valgrind

# MPI para el diseño 4, + environment-modules para exponer mpic++/mpirun en el PATH
sudo dnf install -y openmpi openmpi-devel environment-modules

# Activar y habilitar el servicio Docker (el usuario ya está en el grupo docker,
# así que NO hace falta usermod)
sudo systemctl enable --now docker
```

Nota: si en algún momento el usuario de trabajo no perteneciera al grupo
`docker` (no es el caso aquí, ya se verificó con `id`), el comando sería:

```bash
sudo usermod -aG docker $USER
# luego cerrar sesión y volver a entrar (o `newgrp docker`) para que aplique
```

Después de instalar `openmpi`/`environment-modules`, para usar `mpic++`/`mpirun`
en una sesión de shell (no requiere `sudo`):

```bash
module load mpi/openmpi-x86_64
```

(o añadirlo a `~/.bashrc` si se quiere disponible en cada sesión; esto se
confirmará exactamente cuando se instale, el nombre del módulo puede variar
ligeramente).

## 4. Estado de git

- El repositorio ya estaba clonado desde GitHub
  (`origin` → `https://github.com/Jdavidruanob/mp2.git`), por lo que no se
  ejecutó `git init` ni se tocó el remoto.
- Se creó `.gitignore` para C++ (ejecutables de cada diseño, `*.o`, `output/`
  para las salidas generadas, artefactos de valgrind y de editor).
- Se hizo el commit inicial en `main` con el código original del profesor
  (`src/processor.cpp`, `src/filterer.cpp`), `docs/consigna.pdf`, las imágenes
  de prueba y `CLAUDE.md`.
- Aún no se han creado las ramas `diseno-1`, `diseno-2`, `diseno-3`, `diseno-4`
  (se crearán al iniciar cada fase correspondiente, partiendo cada una de la
  anterior, según indica `CLAUDE.md`).

## 5. Pendientes / dudas

- Entorno 100% listo: `g++`, `valgrind`, OpenMP (probado), Docker (activo) y
  MPI (instalado, requiere `module load mpi/openmpi-x86_64` por sesión) ya
  están disponibles.
- Pendiente de la Fase 1: auditar línea por línea `src/processor.cpp` y
  `src/filterer.cpp` y documentar cada error conocido para el informe.
- No se ha creado todavía la estructura de carpetas/Makefile del proyecto
  propio (se hará en la Fase 1, siguiendo la arquitectura de `CLAUDE.md`), ni
  la carpeta `output/` (se crea cuando exista algo que escribir ahí).
