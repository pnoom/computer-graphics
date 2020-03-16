#include <glm/glm.hpp>
#include <iostream>
#include <limits>

class RayTriangleIntersection
{
  public:
    glm::vec3 intersectionPoint;
    float distanceFromPoint;
    ModelTriangle intersectedTriangle;
    bool isSolution;

    RayTriangleIntersection()
    {
      isSolution = false;
      distanceFromPoint = std::numeric_limits<float>::infinity();
    }

    RayTriangleIntersection(glm::vec3 point, float distance, ModelTriangle triangle, bool is_sol)
    {
        intersectionPoint = point;
        distanceFromPoint = distance;
        intersectedTriangle = triangle;
        isSolution = is_sol;
    }
};

std::ostream& operator<<(std::ostream& os, const RayTriangleIntersection& intersection)
{
  glm::vec3 v = intersection.intersectionPoint;
  os << "Intersection is at (" << v[0] << ", " << v[1] << ", " << v[2] << ") on triangle " << intersection.intersectedTriangle << " at a distance of " << intersection.distanceFromPoint << std::endl;
    return os;
}
