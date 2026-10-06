#ifndef FILTRO_SOBEL_H
#define FILTRO_SOBEL_H

#include "Filtro.h"

// Filtro extra (recomendado por la consigna): detección de bordes Sobel.
// Combina dos kernels 3x3 (Gx, Gy) en vez de uno solo, por eso no reutiliza
// Filtro::convolucionar y define su propio bucle de convolución; sigue
// usando la misma política de bordes (replicación, vía Filtro::clampear).
class FiltroSobel : public Filtro {
public:
    void aplicar(const Imagen &entrada, Imagen &salida,
                 int y0, int y1, int x0, int x1) const override;
    const char *nombre() const override;
};

#endif
