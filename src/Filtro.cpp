#include "Filtro.h"
#include <cmath>

int Filtro::clampear(int valor, int minimo, int maximo) {
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

void Filtro::convolucionar(const Imagen &entrada, Imagen &salida,
                            int y0, int y1, int x0, int x1,
                            const float kernel[3][3], bool valorAbsoluto) {
    int ancho = entrada.getAncho();
    int alto = entrada.getAlto();
    int canales = entrada.getCanales();
    int valorMax = entrada.getValorMax();

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            for (int c = 0; c < canales; c++) {
                float suma = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        // Replicación de borde: el vecino fuera de la
                        // imagen se reemplaza por el píxel válido más
                        // cercano, en vez de ignorarlo o renormalizar
                        // pesos.
                        int nx = clampear(x + kx, 0, ancho - 1);
                        int ny = clampear(y + ky, 0, alto - 1);
                        suma += static_cast<float>(entrada.obtenerValor(nx, ny, c)) * kernel[ky + 1][kx + 1];
                    }
                }
                if (valorAbsoluto) {
                    suma = std::fabs(suma);
                }
                int resultado = clampear(static_cast<int>(suma), 0, valorMax);
                salida.asignarValor(x, y, c, resultado);
            }
        }
    }
}
