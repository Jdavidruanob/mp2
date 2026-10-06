#ifndef FILTRO_FACTORY_H
#define FILTRO_FACTORY_H

#include "Filtro.h"

// Crea el filtro correspondiente a partir de su nombre. Agregar un filtro
// nuevo solo requiere una clase que herede de Filtro y una línea más aquí
// (ver docs/reportes/FASE_2.md, sección "Cómo agregar un filtro nuevo").
class FiltroFactory {
public:
    // Devuelve nullptr si 'nombre' no corresponde a ningún filtro conocido.
    // El llamador es responsable de liberar el filtro devuelto (delete).
    static Filtro *crear(const char *nombre);
};

#endif
