// Diseño 1: aplicación base. Lee una imagen PGM o PPM (detectando el
// formato automáticamente) y la vuelve a escribir, conservando su formato.
#include <cstdio>

#include "Imagen.h"
#include "ImagenIO.h"

int main(int argc, char *argv[]) {
    // El código original validaba argc<2, lo que dejaba pasar
    // invocaciones sin archivo de salida (argv[2] inexistente) y causaba
    // comportamiento indefinido al leerlo.
    if (argc < 3) {
        std::fprintf(stderr, "Uso: %s <entrada.pgm|entrada.ppm|-> <salida>\n", argv[0]);
        std::fprintf(stderr, "Use '-' como entrada para leer desde la entrada estandar (stdin).\n");
        return 1;
    }

    Imagen *imagen = ImagenIO::leer(argv[1]);
    if (imagen == nullptr) {
        return 1;
    }

    std::printf("Leida '%s': %s %dx%d, %d canal(es), max=%d\n",
                argv[1], imagen->nombreFormato(), imagen->getAncho(),
                imagen->getAlto(), imagen->getCanales(), imagen->getValorMax());

    bool ok = ImagenIO::escribir(*imagen, argv[2]);
    delete imagen;

    if (!ok) {
        return 1;
    }

    std::printf("Escrita '%s'\n", argv[2]);
    return 0;
}
