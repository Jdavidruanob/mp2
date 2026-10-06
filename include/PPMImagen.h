#ifndef PPM_IMAGEN_H
#define PPM_IMAGEN_H

#include "Imagen.h"

// Imagen a color (formato P3), tres canales (R, G, B).
class PPMImagen : public Imagen {
public:
    PPMImagen(int ancho, int alto, int valorMax);

    const char *numeroMagico() const override;
    const char *nombreFormato() const override;
    Imagen *crearVacia(int ancho, int alto, int valorMax) const override;
};

#endif
