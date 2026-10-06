#include "FiltroLaplace.h"

void FiltroLaplace::aplicar(const Imagen &entrada, Imagen &salida,
                             int y0, int y1, int x0, int x1) const {
    static const float kernel[3][3] = {
        {0.0f, -1.0f, 0.0f},
        {-1.0f, 4.0f, -1.0f},
        {0.0f, -1.0f, 0.0f},
    };
    // valorAbsoluto = true: ver justificación en el encabezado y en el reporte.
    convolucionar(entrada, salida, y0, y1, x0, x1, kernel, true);
}

const char *FiltroLaplace::nombre() const { return "laplace"; }
