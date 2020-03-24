
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

std::ostream& operator<<(std::ostream& os, const GObject& gobject)
{
    os << "GObject: name " << gobject.name << " colour " << gobject.colour << " num_faces " << gobject.faces.size() << endl;
    return os;
}
