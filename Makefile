CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I$(INC_DIR)
PTHREAD_FLAGS = -pthread
OMP_FLAGS = -fopenmp

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

PROCESSOR_OBJS = $(COMUNES_OBJS) $(SRC_DIR)/processor.o
FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(CLI_OBJS) $(SRC_DIR)/EjecutorSecuencial.o $(SRC_DIR)/filterer.o
TH_FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(CLI_OBJS) $(SRC_DIR)/EjecutorPthreads.o $(SRC_DIR)/th_filterer.o
OMP_FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(CLI_OBJS) $(SRC_DIR)/EjecutorOpenMP.o $(SRC_DIR)/omp_filterer.o

.PHONY: all clean

all: processor filterer th_filterer omp_filterer

processor: $(PROCESSOR_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

filterer: $(FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

th_filterer: $(TH_FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -o $@ $^

omp_filterer: $(OMP_FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) -o $@ $^

# EjecutorPthreads.cpp y EjecutorOpenMP.cpp necesitan flags de compilación
# especiales; estas reglas explícitas tienen prioridad sobre el patrón
# genérico %.o de abajo.
$(SRC_DIR)/EjecutorPthreads.o: $(SRC_DIR)/EjecutorPthreads.cpp
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -c $< -o $@

$(SRC_DIR)/EjecutorOpenMP.o: $(SRC_DIR)/EjecutorOpenMP.cpp
	$(CXX) $(CXXFLAGS) $(OMP_FLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o processor filterer th_filterer omp_filterer
