#include <iostream>

class TexturePoint
{
  public:
    string textureName;
    float x;
    float y;

    TexturePoint()
    {
    }

    TexturePoint(float xPos, float yPos)
    {
      x = xPos;
      y = yPos;
      textureName = "UNASSIGNED";
    }

    TexturePoint(float xPos, float yPos, string tn)
    {
      x = xPos;
      y = yPos;
      textureName = tn;
    }

    void print()
    {
    }
};

std::ostream& operator<<(std::ostream& os, const TexturePoint& point)
{
    os << "(" << point.x << ", " << point.y << ")" << std::endl;
    return os;
}
