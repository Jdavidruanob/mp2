#include "EjecutorSecuencial.h"

void EjecutorSecuencial::ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const {
    filtro.aplicar(entrada, salida, 0, entrada.getAlto(), 0, entrada.getAncho());
}

const char *EjecutorSecuencial::nombre() const { return "secuencial"; }
