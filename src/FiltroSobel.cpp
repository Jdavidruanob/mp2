#include "FiltroSobel.h"
#include <cmath>

void FiltroSobel::aplicar(const Imagen &entrada, Imagen &salida,
                           int y0, int y1, int x0, int x1) const {
    static const float kernelX[3][3] = {
        {-1.0f, 0.0f, 1.0f},
        {-2.0f, 0.0f, 2.0f},
        {-1.0f, 0.0f, 1.0f},
    };
    static const float kernelY[3][3] = {
        {-1.0f, -2.0f, -1.0f},
        {0.0f, 0.0f, 0.0f},
        {1.0f, 2.0f, 1.0f},
    };

    int ancho = entrada.getAncho();
    int alto = entrada.getAlto();
    int canales = entrada.getCanales();
    int valorMax = entrada.getValorMax();

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            for (int c = 0; c < canales; c++) {
                float gx = 0.0f;
                float gy = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int nx = clampear(x + kx, 0, ancho - 1);
                        int ny = clampear(y + ky, 0, alto - 1);
                        float valor = static_cast<float>(entrada.obtenerValor(nx, ny, c));
                        gx += valor * kernelX[ky + 1][kx + 1];
                        gy += valor * kernelY[ky + 1][kx + 1];
                    }
                }
                float magnitud = std::sqrt(gx * gx + gy * gy);
                int resultado = clampear(static_cast<int>(magnitud), 0, valorMax);
                salida.asignarValor(x, y, c, resultado);
            }
        }
    }
}

const char *FiltroSobel::nombre() const { return "sobel"; }
