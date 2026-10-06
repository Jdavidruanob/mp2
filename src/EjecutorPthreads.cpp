#include "EjecutorPthreads.h"
#include <pthread.h>

namespace {

// Lo que recibe cada hilo: el filtro y las imágenes son punteros a datos
// de solo lectura compartidos (entrada) o a una región disjunta de
// escritura (salida); no hay dato mutable compartido entre hilos.
struct ArgumentoHilo {
    const Filtro *filtro;
    const Imagen *entrada;
    Imagen *salida;
    int y0, y1, x0, x1;
};

void *filtrarRegion(void *argPtr) {
    ArgumentoHilo *arg = static_cast<ArgumentoHilo *>(argPtr);
    arg->filtro->aplicar(*arg->entrada, *arg->salida, arg->y0, arg->y1, arg->x0, arg->x1);
    return nullptr;
}

} // namespace

void EjecutorPthreads::ejecutar(const Filtro &filtro, const Imagen &entrada, Imagen &salida) const {
    int ancho = entrada.getAncho();
    int alto = entrada.getAlto();
    // División entera: con dimensiones impares, un cuadrante queda con
    // una fila/columna más que el otro, pero la unión de los 4 sigue
    // cubriendo exactamente [0,ancho) x [0,alto) sin huecos ni solapes.
    int midX = ancho / 2;
    int midY = alto / 2;

    ArgumentoHilo argumentos[4] = {
        {&filtro, &entrada, &salida, 0, midY, 0, midX},        // arriba-izquierda
        {&filtro, &entrada, &salida, 0, midY, midX, ancho},    // arriba-derecha
        {&filtro, &entrada, &salida, midY, alto, 0, midX},     // abajo-izquierda
        {&filtro, &entrada, &salida, midY, alto, midX, ancho}, // abajo-derecha
    };

    pthread_t hilos[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&hilos[i], nullptr, filtrarRegion, &argumentos[i]);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(hilos[i], nullptr);
    }
}

const char *EjecutorPthreads::nombre() const { return "pthreads-4-cuadrantes"; }
