#ifndef EJECUTOR_PTHREADS_H
#define EJECUTOR_PTHREADS_H

#include "EjecutorFiltro.h"

// Divide la imagen en 4 cuadrantes (arriba-izquierda, arriba-derecha,
// abajo-izquierda, abajo-derecha) y lanza un hilo pthread por cuadrante,
// cada uno ejecutando Filtro::aplicar sobre su región. Maneja dimensiones
// impares dividiendo por el medio con división entera (ver .cpp). La
// justificación de por qué no hace falta ningún mutex está en
// docs/reportes/FASE_3.md.
class EjecutorPthreads : public EjecutorFiltro {
public:
    void ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const override;
    const char *nombre() const override;
};

#endif
