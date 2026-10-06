#ifndef FILTRO_BLUR_H
#define FILTRO_BLUR_H

#include "Filtro.h"

// Suavizado: promedio simple 3x3 (todos los pesos 1/9, suman 1).
class FiltroBlur : public Filtro {
public:
    void aplicar(const Imagen &entrada, Imagen &salida,
                 int y0, int y1, int x0, int x1) const override;
    const char *nombre() const override;
};

#endif
