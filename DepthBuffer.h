#include <limits>

class DepthBuffer {
  public:
    float* depthbuf;
    int width, height;

    DepthBuffer(int w, int h){
      width = w;
      height = h;

      depthbuf = (float*)malloc(width*height*sizeof(float));
      std::cout << "depth buffer allocated, size " << width << " by " << height << '\n';
      instantiateDepthBuf();
    }

    bool update(CanvasPoint pixel) {
      int px = round(pixel.x);
      int py = round(pixel.y);
      if (!((py >= 0) && (py < height) && (px >= 0) && (px < width))) return false;
      int invz = 1 / pixel.depth;
      if (invz < depthbuf[py*width + px]) {
        depthbuf[py*width + px] = invz;
        return true;
      }
      else return false;
    }

  private:
    void instantiateDepthBuf() {
      for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
          depthbuf[(j * width) + i] = std::numeric_limits<float>::infinity();
        }
      }
    }
};
