#ifndef TEMPORIZADOR_H
#define TEMPORIZADOR_H

#include <chrono>
#include <ctime>

// Cronómetro simple: mide tiempo real (reloj de pared, std::chrono) y
// tiempo de CPU (clock(), suma de todos los núcleos usados por el proceso)
// desde la última llamada a iniciar(). Se usa una instancia por fase
// (lectura, filtrado, escritura) para poder reportarlas por separado.
class Temporizador {
private:
    std::chrono::steady_clock::time_point inicioReal;
    std::clock_t inicioCPU;

public:
    void iniciar();

    // Segundos de reloj de pared transcurridos desde iniciar().
    double segundosReales() const;

    // Segundos de CPU transcurridos desde iniciar() (clock() / CLOCKS_PER_SEC).
    // En programas con varios hilos activos, esto puede superar el tiempo
    // real porque clock() suma el tiempo de todos los núcleos.
    double segundosCPU() const;
};

#endif
