#include <glm/glm.hpp>
#include <string>

using namespace glm;
using namespace std;

class Camera {
  public:
    vec3 position = vec3(0.0, 2.0, 10.0);
    //mat3 orientation = mat3(1.0F);
    mat3 orientation = mat3(-1, 0, 0,    // right
			    0, -1, 0,    // up
			    0, 0, -1);  // forward
    float focalLength = position[2] / 2;

    Camera () {}

    void printCamera() {
      printCameraPosition();
      printCameraOrientation();
    }

  private:

    void printVec3(vec3 v) {
      cout << "(" << v.x << ", " << v.y << ", " << v.z << ")\n";
    }

    void printCameraPosition() {
      cout << "CAMERA position: \n";
      printVec3(position);
    }

    void printCameraOrientation() {
      cout << "CAMERA orientation:\n";
      for (int i = 0; i < 3; i++) printVec3(orientation[i]);
    }

};
