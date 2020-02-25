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

  private:
    void instantiateDepthBuf() {
      for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
          depthbuf[(j * width) + i] = std::numeric_limits<float>::infinity();
        }
      }
    }
};
