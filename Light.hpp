#include <glm/glm.hpp>

class Light
{
  public:
    glm::vec3 Position = glm::vec3(250.0f, 470.0f, 120.0f);
    //vec3 lightPos(250.0f, 400.0f, 200.0f);
    float Intensity = 2000.0f;
    float Spread = 4.0f;

    Light () {}

    float getIntensityAtPoint(glm::vec3 point) {
      glm::vec3 point_to_light = -Position + point;
      float intensityAtPoint =  Intensity / (Spread * M_PI * glm::length(point_to_light));
      return intensityAtPoint;
    }
};
