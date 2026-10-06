#ifndef EJECUTOR_SECUENCIAL_H
#define EJECUTOR_SECUENCIAL_H

#include "EjecutorFiltro.h"

// Filtra la imagen completa en una sola llamada a Filtro::aplicar, sin
// paralelismo. La usa 'filterer' (diseño 2) y sirve de referencia para
// medir el speedup de las estrategias paralelas (diseños 3 y 4).
class EjecutorSecuencial : public EjecutorFiltro {
public:
    void ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const override;
    const char *nombre() const override;
};

#endif
