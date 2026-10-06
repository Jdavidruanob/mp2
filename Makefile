CXX = g++
MPICXX = mpic++
# OPT: vacío por defecto (sin optimizar, útil para depurar). Para medir
# tiempos reales (scripts/benchmark.sh) se compila con "make OPT=-O2 ...".
OPT =
CXXFLAGS = -Wall -Wextra -std=c++17 $(OPT) -I$(INC_DIR)
PTHREAD_FLAGS = -pthread
OMP_FLAGS = -fopenmp
# OMPI_SKIP_MPICXX: evita que mpi.h arrastre los bindings de C++ de
# OpenMPI (obsoletos, no los usamos: solo la API de C), que de lo
# contrario generan warnings con -Wextra fuera de nuestro código.
MPI_FLAGS = -DOMPI_SKIP_MPICXX

SRC_DIR = src
INC_DIR = include

COMUNES_SRCS = $(SRC_DIR)/Imagen.cpp $(SRC_DIR)/PGMImagen.cpp $(SRC_DIR)/PPMImagen.cpp $(SRC_DIR)/ImagenIO.cpp
COMUNES_OBJS = $(COMUNES_SRCS:.cpp=.o)

FILTROS_SRCS = $(SRC_DIR)/Filtro.cpp $(SRC_DIR)/FiltroBlur.cpp $(SRC_DIR)/FiltroLaplace.cpp \
               $(SRC_DIR)/FiltroSharpen.cpp $(SRC_DIR)/FiltroSobel.cpp $(SRC_DIR)/FiltroFactory.cpp \
               $(SRC_DIR)/Temporizador.cpp
FILTROS_OBJS = $(FILTROS_SRCS:.cpp=.o)

CLI_SRCS = $(SRC_DIR)/EjecucionCLI.cpp
CLI_OBJS = $(CLI_SRCS:.cpp=.o)

MPI_SRCS = $(SRC_DIR)/ParticionMPI.cpp
MPI_OBJS = $(MPI_SRCS:.cpp=.o)

PROCESSOR_OBJS = $(COMUNES_OBJS) $(SRC_DIR)/processor.o
FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(CLI_OBJS) $(SRC_DIR)/EjecutorSecuencial.o $(SRC_DIR)/filterer.o
TH_FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(CLI_OBJS) $(SRC_DIR)/EjecutorPthreads.o $(SRC_DIR)/th_filterer.o
OMP_FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(CLI_OBJS) $(SRC_DIR)/EjecutorOpenMP.o $(SRC_DIR)/omp_filterer.o
MPI_FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(MPI_OBJS) $(SRC_DIR)/mpi_filterer.o

.PHONY: all clean

# mpi_filterer queda fuera de "all" a propósito: necesita mpic++ en el
# PATH (en Fedora, "module load mpi/openmpi-x86_64"; en la imagen Docker
# de docker-compose.yml ya está en el PATH por defecto). Compilar con
# "make mpi_filterer".
all: processor filterer th_filterer omp_filterer

processor: $(PROCESSOR_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

filterer: $(FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

th_filterer: $(TH_FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -o $@ $^

omp_filterer: $(OMP_FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) -o $@ $^

mpi_filterer: $(MPI_FILTERER_OBJS)
	$(MPICXX) $(CXXFLAGS) $(MPI_FLAGS) -o $@ $^

# EjecutorPthreads.cpp, EjecutorOpenMP.cpp y mpi_filterer.cpp necesitan
# compilación especial (flags o compilador distinto); estas reglas
# explícitas tienen prioridad sobre el patrón genérico %.o de abajo.
$(SRC_DIR)/EjecutorPthreads.o: $(SRC_DIR)/EjecutorPthreads.cpp
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -c $< -o $@

$(SRC_DIR)/EjecutorOpenMP.o: $(SRC_DIR)/EjecutorOpenMP.cpp
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) -c $< -o $@

$(SRC_DIR)/mpi_filterer.o: $(SRC_DIR)/mpi_filterer.cpp
	$(MPICXX) $(CXXFLAGS) $(MPI_FLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o processor filterer th_filterer omp_filterer mpi_filterer
