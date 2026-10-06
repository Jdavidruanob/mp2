// Diseño 2: versión secuencial del filtrado.
// Uso: ./filterer entrada salida [--f <blur|laplace|sharpen|sobel>]
#include "EjecucionCLI.h"
#include "EjecutorSecuencial.h"

int main(int argc, char *argv[]) {
    EjecutorSecuencial ejecutor;
    return ejecutarCLI(argc, argv, ejecutor);
}
