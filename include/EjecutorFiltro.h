#ifndef EJECUTOR_FILTRO_H
#define EJECUTOR_FILTRO_H

#include "Filtro.h"
#include "Imagen.h"

// Estrategia de ejecución: decide CÓMO se recorre la imagen para aplicar
// un filtro (secuencial, pthreads por cuadrantes, OpenMP por filas, y en
// el futuro MPI por franjas), pero siempre delega el cálculo de cada
// píxel a Filtro::aplicar sobre la región que le corresponda. Ninguna
// estrategia reimplementa la convolución: solo deciden la partición del
// trabajo y, si aplica, cómo repartirlo entre hilos/procesos.
class EjecutorFiltro {
public:
    virtual ~EjecutorFiltro() = default;

    // Aplica 'filtro' a la imagen completa 'entrada', escribiendo en 'salida'
    // (misma dimensiones/canales). 'entrada' es de solo lectura y compartida;
    // cada estrategia debe garantizar que las distintas partes del trabajo
    // escriban regiones disjuntas de 'salida'.
    virtual void ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const = 0;

    // Identifica la estrategia (para el CSV de tiempos), p. ej.
    // "secuencial", "pthreads-4-cuadrantes", "openmp-4-hilos".
    virtual const char *nombre() const = 0;
};

#endif
