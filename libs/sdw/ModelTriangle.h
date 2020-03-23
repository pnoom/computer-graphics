#include <glm/glm.hpp>
#include "Colour.h"
#include <string>
#include <optional>
#include "TextureTriangle.hpp"

using namespace std;

class ModelTriangle
{
  public:
    glm::vec3 vertices[3];
    Colour colour;

    // This value will only exist for some ModelTriangles
    optional<TextureTriangle> maybeTextureTriangle;

    // If this ModelTriangle does not have a TextureTriangle, this vector
    // will always be empty.
    // Otherwise, it will be empty until we calculate the exact TexturePoints
    // needed. (Actually, consider not storing these at all: just calculate and
    // draw directly to screen).
    // vector<TexturePoint> texturePoints;

    ModelTriangle()
    {
    }

    ModelTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Colour trigColour)
    {
      vertices[0] = v0;
      vertices[1] = v1;
      vertices[2] = v2;
      colour = trigColour;
    }
};

std::ostream& operator<<(std::ostream& os, const ModelTriangle& triangle)
{
    os << "(" << triangle.vertices[0].x << ", " << triangle.vertices[0].y << ", " << triangle.vertices[0].z << ")" << std::endl;
    os << "(" << triangle.vertices[1].x << ", " << triangle.vertices[1].y << ", " << triangle.vertices[1].z << ")" << std::endl;
    os << "(" << triangle.vertices[2].x << ", " << triangle.vertices[2].y << ", " << triangle.vertices[2].z << ")" << std::endl;
    os << std::endl;
    return os;
}
