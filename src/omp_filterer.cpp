// Diseño 3 (OpenMP): "#pragma omp parallel for" sobre las filas (ver
// EjecutorOpenMP).
// Uso: ./omp_filterer entrada salida [--f <blur|laplace|sharpen|sobel>] [--hilos N]
// Sin --hilos se usa la variable de entorno OMP_NUM_THREADS, o el valor
// por defecto del runtime de OpenMP si tampoco está definida.
#include "EjecucionCLI.h"
#include "EjecutorOpenMP.h"

#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[]) {
    int numHilos = 0; // <= 0: dejar que decida OpenMP (OMP_NUM_THREADS o el valor por defecto)
    for (int i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--hilos") == 0 && i + 1 < argc) {
            numHilos = std::atoi(argv[i + 1]);
        }
    }

    EjecutorOpenMP ejecutor(numHilos);
    return ejecutarCLI(argc, argv, ejecutor);
}
