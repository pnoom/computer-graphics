#pragma once

#include <string>
#include <fstream>

class Texture {
  public:
    uint32_t* ppm_image;
    int width, height;
    int maxcolour;

    Texture() {}

    Texture(std::string imageName) {
      FILE* f = fopen(imageName.c_str(), "r");
      char buf[64];

      if (f == NULL) {
        std::cout << "Could not open file." << '\n';
        exit(1);
      }

      fgets(buf, 64, f); // P6
      fgets(buf, 64, f); // could be GIMP comment
      if (buf[0] == '#') {
        fgets(buf, 64, f); // definitely the dimensions
      }

      sscanf(buf, "%d %d", &width, &height);
      fgets(buf, 64, f); // bit depth, should be 255
      sscanf(buf, "%d", &maxcolour);
      if (maxcolour != 255) {
        std::cout << "Unsupported bit depth. Use 255 plz." << '\n';
        exit(1);
      }

      uint8_t linebuf[3 * width];
      ppm_image = (uint32_t*)malloc(width*height*sizeof(uint32_t));

      int y = 0;
      while(!feof(f)) {
        fread(&linebuf, 1, 3*width, f);
        for (int i = 0; i < width; i++) {
          uint32_t colour = (0x00 << 24) + (linebuf[3*i] << 16) + (linebuf[3*i + 1] << 8) + linebuf[3*i + 2];
          ppm_image[y*width + i] = colour;
        }
        y++;
      }
      fclose(f);
    }
};
