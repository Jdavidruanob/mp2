#ifndef FILTRO_H
#define FILTRO_H

#include "Imagen.h"

// Filtro de convolución 3x3, abstracto. La región se expresa en
// semiabierto [y0,y1) x [x0,x1): filas y0..y1-1, columnas x0..x1-1.
//
// 'entrada' es de solo lectura y 'salida' ya debe existir con las mismas
// dimensiones/canales que 'entrada' (ver Imagen::crearVacia). Cada
// implementación escribe únicamente dentro de la región indicada de
// 'salida', pero puede LEER cualquier posición de 'entrada' (incluso fuera
// de la región) para resolver sus vecinos de borde. Esto es lo que permite
// que, en los diseños 3 y 4, distintos hilos/procesos filtren regiones
// distintas de la misma imagen de entrada sin condición de carrera: cada
// uno solo escribe su propia región de salida.
class Filtro {
public:
    virtual ~Filtro() = default;

    virtual void aplicar(const Imagen &entrada, Imagen &salida,
                          int y0, int y1, int x0, int x1) const = 0;

    // Nombre corto usado en la CLI y en FiltroFactory ("blur", "laplace", ...).
    virtual const char *nombre() const = 0;

protected:
    static int clampear(int valor, int minimo, int maximo);

    // Convolución genérica de un único kernel 3x3. Política de bordes:
    // replicación (clamp de las coordenadas del vecino a los límites de
    // la imagen), NO normalización por pesos válidos. Cada canal se
    // convoluciona de forma independiente. Si 'valorAbsoluto' es true, se
    // toma |resultado| antes del clamp final a [0, valorMax] (lo usa
    // Laplace, que produce valores negativos).
    static void convolucionar(const Imagen &entrada, Imagen &salida,
                               int y0, int y1, int x0, int x1,
                               const float kernel[3][3], bool valorAbsoluto);
};

#endif
