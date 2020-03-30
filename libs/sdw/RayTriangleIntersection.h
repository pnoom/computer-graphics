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
    glm::vec3 e0;
    glm::vec3 e1;
    float u, v;

    RayTriangleIntersection()
    {
      isSolution = false;
      distanceFromPoint = std::numeric_limits<float>::infinity();
    }

    RayTriangleIntersection(glm::vec3 point, float distance, ModelTriangle triangle, bool is_sol, float RTI_u, float RTI_v)
    {
        intersectionPoint = point;
        distanceFromPoint = distance;
        intersectedTriangle = triangle;
        isSolution = is_sol;
        u = RTI_u;
        v = RTI_v;
        e0 = intersectedTriangle.vertices[1] - intersectedTriangle.vertices[0];
        e1 = intersectedTriangle.vertices[2] - intersectedTriangle.vertices[0];
    }
};

std::ostream& operator<<(std::ostream& os, const RayTriangleIntersection& intersection)
{
  glm::vec3 v = intersection.intersectionPoint;
  os << "Intersection is at (" << v[0] << ", " << v[1] << ", " << v[2] << ") on triangle " << intersection.intersectedTriangle << " at a distance of " << intersection.distanceFromPoint << std::endl;
    return os;
}
