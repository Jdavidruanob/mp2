CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I$(INC_DIR)

SRC_DIR = src
INC_DIR = include

COMUNES_SRCS = $(SRC_DIR)/Imagen.cpp $(SRC_DIR)/PGMImagen.cpp $(SRC_DIR)/PPMImagen.cpp $(SRC_DIR)/ImagenIO.cpp
COMUNES_OBJS = $(COMUNES_SRCS:.cpp=.o)

FILTROS_SRCS = $(SRC_DIR)/Filtro.cpp $(SRC_DIR)/FiltroBlur.cpp $(SRC_DIR)/FiltroLaplace.cpp \
               $(SRC_DIR)/FiltroSharpen.cpp $(SRC_DIR)/FiltroSobel.cpp $(SRC_DIR)/FiltroFactory.cpp \
               $(SRC_DIR)/Temporizador.cpp
FILTROS_OBJS = $(FILTROS_SRCS:.cpp=.o)

PROCESSOR_OBJS = $(COMUNES_OBJS) $(SRC_DIR)/processor.o
FILTERER_OBJS = $(COMUNES_OBJS) $(FILTROS_OBJS) $(SRC_DIR)/filterer.o

.PHONY: all clean

all: processor filterer

processor: $(PROCESSOR_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

filterer: $(FILTERER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o processor filterer
