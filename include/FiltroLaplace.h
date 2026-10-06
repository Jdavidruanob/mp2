#ifndef FILTRO_LAPLACE_H
#define FILTRO_LAPLACE_H

#include "Filtro.h"

// Detección de bordes: Laplaciano de 4 vecinos. El kernel suma 0, así que
// el resultado puede ser negativo; se trata con valor absoluto (ver
// Filtro::convolucionar y docs/reportes/FASE_2.md).
class FiltroLaplace : public Filtro {
public:
    void aplicar(const Imagen &entrada, Imagen &salida,
                 int y0, int y1, int x0, int x1) const override;
    const char *nombre() const override;
};

#endif
