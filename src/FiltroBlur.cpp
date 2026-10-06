#include "FiltroBlur.h"

void FiltroBlur::aplicar(const Imagen &entrada, Imagen &salida,
                          int y0, int y1, int x0, int x1) const {
    static const float kernel[3][3] = {
        {1.0f / 9, 1.0f / 9, 1.0f / 9},
        {1.0f / 9, 1.0f / 9, 1.0f / 9},
        {1.0f / 9, 1.0f / 9, 1.0f / 9},
    };
    convolucionar(entrada, salida, y0, y1, x0, x1, kernel, false);
}

const char *FiltroBlur::nombre() const { return "blur"; }
