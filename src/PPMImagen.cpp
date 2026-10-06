#include "PPMImagen.h"

PPMImagen::PPMImagen(int ancho, int alto, int valorMax)
    : Imagen(ancho, alto, valorMax, 3) {}

const char *PPMImagen::numeroMagico() const { return "P3"; }
const char *PPMImagen::nombreFormato() const { return "PPM"; }

Imagen *PPMImagen::crearVacia(int ancho, int alto, int valorMax) const {
    return new PPMImagen(ancho, alto, valorMax);
}
