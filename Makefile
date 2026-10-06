CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I$(INC_DIR)

SRC_DIR = src
INC_DIR = include

COMUNES_SRCS = $(SRC_DIR)/Imagen.cpp $(SRC_DIR)/PGMImagen.cpp $(SRC_DIR)/PPMImagen.cpp $(SRC_DIR)/ImagenIO.cpp
COMUNES_OBJS = $(COMUNES_SRCS:.cpp=.o)

PROCESSOR_OBJS = $(COMUNES_OBJS) $(SRC_DIR)/processor.o

.PHONY: all clean

all: processor

processor: $(PROCESSOR_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o processor
