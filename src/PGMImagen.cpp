#include "PGMImagen.h"

PGMImagen::PGMImagen(int ancho, int alto, int valorMax)
    : Imagen(ancho, alto, valorMax, 1) {}

const char *PGMImagen::numeroMagico() const { return "P2"; }
const char *PGMImagen::nombreFormato() const { return "PGM"; }

Imagen *PGMImagen::crearVacia(int ancho, int alto, int valorMax) const {
    return new PGMImagen(ancho, alto, valorMax);
}
