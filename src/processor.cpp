#include <iostream>

#define MAX_FILENAME 256
#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {

  char buffer[BUFFER_SIZE];
  char magic[3];
  int width;
  int height;
  int max_color;
  int *pixels;

  if(argc<2){
    std::cout << "missing input and output paths\n";
    std::cout << "usage:" << argv[0] << " input_image.pgm output_image.pgm" << std::endl;
    std::cout << "or "<< argv[0] << "input_image.ppm output_image.ppm" << std::endl;
    return 1;
  }
  // abrir archivo
  FILE *file = fopen(argv[1], "r");
  if (file == NULL) {
    std::cout << "Error, incorrect path or incorrect file."<< std::endl;
    return 1;
  }
  // Leer y mostrar línea por línea
  fscanf(file, "%2s", magic);  // magic number
  fscanf(file, "%d %d", &width, &height);
  fscanf(file, "%d", &max_color);

  int pixel_count = width * height;
  if (strcmp(magic, "P3") != 0){
    int pixel_count = width * height * 3;
  }

  pixels = (int *) malloc(pixel_count);
  int value;

  for (int i = 0; i < pixel_count; i++) {
      if (fscanf(file, "%d", &value) != 1) {
        std::cout << "Error reading pixels."<< std::endl;
        free(pixels);
        fclose(file);
        return 0;
      }
      pixels[i] = value;
  }
  fclose(file);

  FILE * output = fopen(argv[2], "w");
  (void) fprintf(output, "%s\n%d %d\n%d\n", magic, width, height, max_color);
  for (int i = 0; i < pixel_count; i++) {
    fprintf(output, "%d\n", pixels[i]);
  }
  (void) fclose(output);
  return 0;


  
  

}