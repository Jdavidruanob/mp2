#include "Imagen.h"
#include <cstdlib>

Imagen::Imagen(int ancho, int alto, int valorMax, int canales)
    : ancho(ancho), alto(alto), valorMax(valorMax), canales(canales) {
    // Arreglo dinámico de enteros: el tamaño en bytes es
    // getCantidadValores() * sizeof(int), no simplemente getCantidadValores().
    // (Ese era el primer error del código del profesor: malloc(pixel_count)
    // reservaba bytes en vez de enteros.)
    pixeles = new int[getCantidadValores()];
}

Imagen::~Imagen() {
    delete[] pixeles;
}

int Imagen::getAncho() const { return ancho; }
int Imagen::getAlto() const { return alto; }
int Imagen::getValorMax() const { return valorMax; }
int Imagen::getCanales() const { return canales; }

int Imagen::getCantidadValores() const {
    return ancho * alto * canales;
}

int *Imagen::getPixeles() { return pixeles; }
const int *Imagen::getPixeles() const { return pixeles; }

int Imagen::obtenerValor(int x, int y, int canal) const {
    return pixeles[(y * ancho + x) * canales + canal];
}

void Imagen::asignarValor(int x, int y, int canal, int valor) {
    pixeles[(y * ancho + x) * canales + canal] = valor;
}
