#ifndef PGM_IMAGEN_H
#define PGM_IMAGEN_H

#include "Imagen.h"

// Imagen en escala de grises (formato P2), un solo canal.
class PGMImagen : public Imagen {
public:
    PGMImagen(int ancho, int alto, int valorMax);

    const char *numeroMagico() const override;
    const char *nombreFormato() const override;
    Imagen *crearVacia(int ancho, int alto, int valorMax) const override;
};

#endif
