

#include <string>
#include <vector>

class GObject {
  public:
    std::string name;
    Colour colour;
    std::vector<ModelTriangle> faces;
    
    GObject () {}

    GObject (std::string n, Colour c, std::vector<ModelTriangle> fs) {
      name = n;
      colour = c;
      faces = fs;
    }
  
};
