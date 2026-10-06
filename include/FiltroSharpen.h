#ifndef FILTRO_SHARPEN_H
#define FILTRO_SHARPEN_H

#include "Filtro.h"

// Realce (sharpening): el centro se refuerza y se resta el promedio de
// los 4 vecinos ortogonales (pesos suman 1).
class FiltroSharpen : public Filtro {
public:
    void aplicar(const Imagen &entrada, Imagen &salida,
                 int y0, int y1, int x0, int x1) const override;
    const char *nombre() const override;
};

#endif
