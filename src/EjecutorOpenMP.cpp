#include "EjecutorOpenMP.h"
#include <cstdio>
#include <omp.h>

EjecutorOpenMP::EjecutorOpenMP(int numHilos) : numHilos(numHilos) {
    if (numHilos > 0) {
        std::snprintf(etiqueta, sizeof(etiqueta), "openmp-%d-hilos", numHilos);
    } else {
        std::snprintf(etiqueta, sizeof(etiqueta), "openmp-auto");
    }
}

void EjecutorOpenMP::ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const {
    if (numHilos > 0) {
        omp_set_num_threads(numHilos);
    }
    int ancho = entrada.getAncho();
    int alto = entrada.getAlto();

    // Cada iteración del for es una fila completa; distintas iteraciones
    // escriben filas distintas de 'salida' (regiones disjuntas), 'entrada'
    // solo se lee. No hay variables compartidas mutables, por eso no hace
    // falta 'reduction' ni cláusulas de sincronización adicionales.
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < alto; y++) {
        filtro.aplicar(entrada, salida, y, y + 1, 0, ancho);
    }
}

const char *EjecutorOpenMP::nombre() const { return etiqueta; }
