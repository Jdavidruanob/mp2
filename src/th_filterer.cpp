// Diseño 3 (pthreads): la imagen se divide en 4 cuadrantes y un hilo
// filtra cada uno (ver EjecutorPthreads).
// Uso: ./th_filterer entrada salida [--f <blur|laplace|sharpen|sobel>]
#include "EjecucionCLI.h"
#include "EjecutorPthreads.h"

int main(int argc, char *argv[]) {
    EjecutorPthreads ejecutor;
    return ejecutarCLI(argc, argv, ejecutor);
}
