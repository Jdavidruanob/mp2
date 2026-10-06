// Diseño 4 (MPI, memoria distribuida).
// El rank 0 lee la imagen y reparte franjas de filas entre todos los
// ranks, con 1 fila fantasma (halo) arriba y abajo donde corresponda.
// Cada rank aplica los tres filtros obligatorios (blur, laplace, sharpen)
// a su franja reutilizando Filtro::aplicar (igual que los diseños 2 y 3,
// sin reimplementar la convolución), y el rank 0 recolecta y escribe las
// tres salidas. Cada rank imprime su propia línea de tiempos (filtrado y
// comunicación, real y CPU).
//
// Uso: mpirun -np N ./mpi_filterer entrada salida
//
// Fase 6: además de filtrado y comunicación (por cada rank), el rank 0
// también mide e imprime lectura, escritura y total (real y CPU), igual
// que filterer/th_filterer/omp_filterer -- permite comparar el peso de la
// E/S también en la versión MPI, y correr el mismo tipo de benchmark
// dentro del clúster Docker (ver docs/reportes/FASE_6.md).
#include <mpi.h>

#include <cstdio>
#include <cstring>

#include "Imagen.h"
#include "PGMImagen.h"
#include "PPMImagen.h"
#include "ImagenIO.h"
#include "Filtro.h"
#include "FiltroFactory.h"
#include "Temporizador.h"
#include "ParticionMPI.h"

namespace {

void construirNombreSalida(const char *base, const char *sufijo, char *destino, size_t tam) {
    const char *punto = std::strrchr(base, '.');
    if (punto == nullptr) {
        std::snprintf(destino, tam, "%s_%s", base, sufijo);
    } else {
        int prefijoLen = static_cast<int>(punto - base);
        std::snprintf(destino, tam, "%.*s_%s%s", prefijoLen, base, sufijo, punto);
    }
}

Imagen *crearImagenVacia(bool esPGM, int ancho, int alto, int valorMax) {
    if (esPGM) {
        return new PGMImagen(ancho, alto, valorMax);
    }
    return new PPMImagen(ancho, alto, valorMax);
}

} // namespace

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank = 0;
    int size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            std::fprintf(stderr, "Uso: mpirun -np N %s <entrada> <salida>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    const char *rutaEntrada = argv[1];
    const char *rutaSalida = argv[2];

    // --- Lectura (solo rank 0) y difusión de metadatos ---
    Imagen *entradaCompleta = nullptr;
    int ancho = 0;
    int alto = 0;
    int canales = 0;
    int valorMax = 0;
    int errorLectura = 0;
    double lecturaReal = 0.0;
    double lecturaCPU = 0.0;

    if (rank == 0) {
        Temporizador tLectura;
        tLectura.iniciar();
        entradaCompleta = ImagenIO::leer(rutaEntrada);
        lecturaReal = tLectura.segundosReales();
        lecturaCPU = tLectura.segundosCPU();
        if (entradaCompleta == nullptr) {
            errorLectura = 1;
        } else {
            ancho = entradaCompleta->getAncho();
            alto = entradaCompleta->getAlto();
            canales = entradaCompleta->getCanales();
            valorMax = entradaCompleta->getValorMax();
        }
    }

    MPI_Bcast(&errorLectura, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (errorLectura) {
        MPI_Finalize();
        return 1;
    }

    MPI_Bcast(&ancho, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&alto, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&canales, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&valorMax, 1, MPI_INT, 0, MPI_COMM_WORLD);
    bool esPGM = (canales == 1);

    // --- Partición determinista: cada rank calcula su propia franja sin
    // necesidad de que rank 0 se la comunique ---
    int y0 = 0;
    int filas = 0;
    calcularParticion(alto, size, rank, y0, filas);

    bool haloArriba = (y0 > 0);
    bool haloAbajo = (y0 + filas < alto);
    int bufferDesde = y0 - (haloArriba ? 1 : 0);
    int bufferHasta = y0 + filas + (haloAbajo ? 1 : 0);
    int filasConHalo = bufferHasta - bufferDesde;
    int offsetReal = haloArriba ? 1 : 0;

    Imagen *entradaLocal = crearImagenVacia(esPGM, ancho, filasConHalo, valorMax);

    // --- Reparto de franjas (con halo) ---
    double comunicacionReal = 0.0;
    double comunicacionCPU = 0.0;
    Temporizador tComunicacion;

    if (rank == 0) {
        tComunicacion.iniciar();
        // Rank 0 copia su propia franja directamente (no se envía un
        // mensaje MPI a sí mismo).
        std::memcpy(entradaLocal->getPixeles(),
                    entradaCompleta->getPixeles() + static_cast<long>(bufferDesde) * ancho * canales,
                    static_cast<size_t>(filasConHalo) * ancho * canales * sizeof(int));

        for (int r = 1; r < size; r++) {
            int y0r = 0;
            int filasR = 0;
            calcularParticion(alto, size, r, y0r, filasR);
            bool haloArribaR = (y0r > 0);
            bool haloAbajoR = (y0r + filasR < alto);
            int bufferDesdeR = y0r - (haloArribaR ? 1 : 0);
            int bufferHastaR = y0r + filasR + (haloAbajoR ? 1 : 0);
            int filasConHaloR = bufferHastaR - bufferDesdeR;
            int cantidadR = filasConHaloR * ancho * canales;

            MPI_Send(entradaCompleta->getPixeles() + static_cast<long>(bufferDesdeR) * ancho * canales,
                      cantidadR, MPI_INT, r, 0, MPI_COMM_WORLD);
        }
        comunicacionReal += tComunicacion.segundosReales();
        comunicacionCPU += tComunicacion.segundosCPU();
    } else {
        tComunicacion.iniciar();
        MPI_Recv(entradaLocal->getPixeles(), filasConHalo * ancho * canales, MPI_INT,
                  0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        comunicacionReal += tComunicacion.segundosReales();
        comunicacionCPU += tComunicacion.segundosCPU();
    }

    // --- Filtrado local: los tres filtros obligatorios sobre la franja
    // propia, reutilizando Filtro::aplicar tal cual lo usan los diseños
    // 2 y 3 ---
    static const char *nombresFiltros[3] = {"blur", "laplace", "sharpen"};
    Imagen *salidasLocales[3] = {nullptr, nullptr, nullptr};

    Temporizador tFiltrado;
    tFiltrado.iniciar();
    for (int i = 0; i < 3; i++) {
        salidasLocales[i] = crearImagenVacia(esPGM, ancho, filasConHalo, valorMax);
        if (filas > 0) {
            Filtro *filtro = FiltroFactory::crear(nombresFiltros[i]);
            filtro->aplicar(*entradaLocal, *salidasLocales[i], offsetReal, offsetReal + filas, 0, ancho);
            delete filtro;
        }
    }
    double filtradoReal = tFiltrado.segundosReales();
    double filtradoCPU = tFiltrado.segundosCPU();

    // --- Recolección de las franjas filtradas (sin halo) en rank 0 ---
    Imagen *salidasFinales[3] = {nullptr, nullptr, nullptr};
    Temporizador tRecoleccion;
    tRecoleccion.iniciar();

    if (rank == 0) {
        for (int i = 0; i < 3; i++) {
            salidasFinales[i] = crearImagenVacia(esPGM, ancho, alto, valorMax);
            if (filas > 0) {
                std::memcpy(salidasFinales[i]->getPixeles() + static_cast<long>(y0) * ancho * canales,
                            salidasLocales[i]->getPixeles() + static_cast<long>(offsetReal) * ancho * canales,
                            static_cast<size_t>(filas) * ancho * canales * sizeof(int));
            }
        }
        for (int r = 1; r < size; r++) {
            int y0r = 0;
            int filasR = 0;
            calcularParticion(alto, size, r, y0r, filasR);
            if (filasR == 0) continue;
            int cantidadR = filasR * ancho * canales;
            for (int i = 0; i < 3; i++) {
                int *destino = salidasFinales[i]->getPixeles() + static_cast<long>(y0r) * ancho * canales;
                MPI_Recv(destino, cantidadR, MPI_INT, r, 10 + i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
    } else if (filas > 0) {
        for (int i = 0; i < 3; i++) {
            int *origen = salidasLocales[i]->getPixeles() + static_cast<long>(offsetReal) * ancho * canales;
            MPI_Send(origen, filas * ancho * canales, MPI_INT, 0, 10 + i, MPI_COMM_WORLD);
        }
    }
    comunicacionReal += tRecoleccion.segundosReales();
    comunicacionCPU += tRecoleccion.segundosCPU();

    // --- Escritura (solo rank 0): las 3 salidas, tiempo sumado entre ellas
    // (igual criterio que filterer/th_filterer/omp_filterer en
    // EjecucionCLI.cpp: se aplican 3 filtros, cada uno con su propio
    // archivo de salida) ---
    bool ok = true;
    double escrituraReal = 0.0;
    double escrituraCPU = 0.0;
    if (rank == 0) {
        for (int i = 0; i < 3; i++) {
            char rutaGenerada[1024];
            construirNombreSalida(rutaSalida, nombresFiltros[i], rutaGenerada, sizeof(rutaGenerada));
            Temporizador tEscritura;
            tEscritura.iniciar();
            bool okUna = ImagenIO::escribir(*salidasFinales[i], rutaGenerada);
            escrituraReal += tEscritura.segundosReales();
            escrituraCPU += tEscritura.segundosCPU();
            ok = okUna && ok;
        }
    }

    // --- Cada rank imprime su propia línea de tiempos. Solo el rank 0
    // mide lectura/escritura/total (es el único que lee y escribe
    // archivos); en el resto esas columnas quedan vacías. ---
    if (rank == 0) {
        std::printf("rank,size,entrada,ancho,alto,filas_propias,lectura_real_s,lectura_cpu_s,filtrado_real_s,filtrado_cpu_s,comunicacion_real_s,comunicacion_cpu_s,escritura_real_s,escritura_cpu_s,total_real_s,total_cpu_s\n");
    }
    MPI_Barrier(MPI_COMM_WORLD); // encabezado antes que las filas de datos

    if (rank == 0) {
        double totalReal = lecturaReal + filtradoReal + comunicacionReal + escrituraReal;
        double totalCPU = lecturaCPU + filtradoCPU + comunicacionCPU + escrituraCPU;
        std::printf("%d,%d,%s,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                    rank, size, rutaEntrada, ancho, alto, filas,
                    lecturaReal, lecturaCPU, filtradoReal, filtradoCPU,
                    comunicacionReal, comunicacionCPU, escrituraReal, escrituraCPU,
                    totalReal, totalCPU);
    } else {
        std::printf("%d,%d,%s,%d,%d,%d,,,%.6f,%.6f,%.6f,%.6f,,,,\n",
                    rank, size, rutaEntrada, ancho, alto, filas,
                    filtradoReal, filtradoCPU, comunicacionReal, comunicacionCPU);
    }
    std::fflush(stdout);

    delete entradaLocal;
    for (int i = 0; i < 3; i++) {
        delete salidasLocales[i];
        delete salidasFinales[i];
    }
    delete entradaCompleta;

    MPI_Finalize();
    return (rank == 0 && !ok) ? 1 : 0;
}
