#include <glm/glm.hpp>

class Light
{
  public:
    glm::vec3 Position = glm::vec3(500.0f, 500.0f, 500.0f);
    //vec3 lightPos(250.0f, 400.0f, 200.0f);
    float Intensity = 10000.0f;
    float Spread = 20.0f;

    Light () {}

    float getIntensityAtPoint(glm::vec3 point) {
      glm::vec3 point_to_light = -Position + point;
      float intensityAtPoint =  Intensity / (Spread * M_PI * glm::length(point_to_light));
      return intensityAtPoint;
    }
};
