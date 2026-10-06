#include "FiltroSharpen.h"

void FiltroSharpen::aplicar(const Imagen &entrada, Imagen &salida,
                             int y0, int y1, int x0, int x1) const {
    static const float kernel[3][3] = {
        {0.0f, -1.0f, 0.0f},
        {-1.0f, 5.0f, -1.0f},
        {0.0f, -1.0f, 0.0f},
    };
    convolucionar(entrada, salida, y0, y1, x0, x1, kernel, false);
}

const char *FiltroSharpen::nombre() const { return "sharpen"; }
