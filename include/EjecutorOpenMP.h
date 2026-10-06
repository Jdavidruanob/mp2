#ifndef EJECUTOR_OPENMP_H
#define EJECUTOR_OPENMP_H

#include "EjecutorFiltro.h"

// Paraleliza con "#pragma omp parallel for" sobre las filas de la imagen:
// cada iteración (una fila) llama a Filtro::aplicar sobre la región
// [y,y+1) x [0,ancho), igual que EjecutorPthreads reutiliza Filtro::aplicar
// sin reimplementar la convolución.
class EjecutorOpenMP : public EjecutorFiltro {
private:
    int numHilos;      // <= 0 significa "dejar que OpenMP decida" (OMP_NUM_THREADS o el valor por defecto)
    char etiqueta[32];

public:
    explicit EjecutorOpenMP(int numHilos);

    void ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const override;
    const char *nombre() const override;
};

#endif
