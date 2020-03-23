#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <tuple>

using namespace std;
using namespace glm;

typedef unordered_map<string, Colour> materialDict;

class OBJ_IO {
  public:
    OBJ_IO () {}

    // For clarity: each ModelTriangle (a triangle in 3D space) may get their
    // "filling" from a TextureTriangle (a triangle in 2D space).

    // Pass 1:
    // First process included .mtl files (build dictionary of materials and/or
    // get the filename of the texture file.) Then go through whole file and:
    // Store all vertices and texture coords in two vectors. (Can't do faces yet
    // since they use global indices for vertices.) (Actually, we CAN, if we
    // just store the indices for now. Then, in second pass, we can build them.
    // The second pass is needed mainly for the "named object" stuff.)

    // Pass 2:
    // Go through whole file again (tracking the indices of vertices, texture
    // points and faces):
    // If no named object (e.g. light, back_wall) has been introduced,
    // associate the current indices with an extra object called "loose".
    // Otherwise, between named object intros: associate the current indices
    // with the current named object.
    // Finally, go through the intermediate structures representing these named
    // objects, building gobjects (first ModelTriangles)
    vector<GObject> loadOBJ(string filename, int WIDTH) {
      vector<GObject> res = loadOBJpass1(filename);
      res = scale(WIDTH, res);
      return res;
    }

  private:
    void skipToNextLine(ifstream& inFile) {
      inFile.ignore(numeric_limits<int>::max(), '\n');
    }

    bool emptyOrCommentLine(string lineString) {
      return (lineString.empty() || lineString.front() == '#');
    }

    tuple<materialDict, string> loadMTL(string filename) {
      // Declarations of stuff we want to parse from (string) input streams
      materialDict mtlDict;
      string textureFilename, mtlName;
      float r, g, b;
      Colour colour;

      ifstream inFile;
      inFile.open(filename);
      if (inFile.fail()) {
        cout << "File not found." << endl;
        exit(1);
      }

      string lineString, linePrefix;

      while (getline(inFile, lineString)) {
        if (emptyOrCommentLine(lineString)) continue;
        istringstream lineStream(lineString);
        lineStream >> linePrefix; // use this as the conditional in an if stmt to detect failure if needed
        if (linePrefix == "map_Kd") {
          lineStream >> textureFilename;
        }
        else if (linePrefix == "newmtl") {
          lineStream >> mtlName;
          skipToNextLine(inFile);
          lineStream >> linePrefix;
          if (linePrefix == "Kd") {
            lineStream >> r >> g >> b;
            mtlDict.insert({mtlName, Colour(mtlName, round(255*r), round(255*g), round(255*b)});
          }
          else {
            cout << "Line after 'newmtl' should be 'Kd r g b'." << endl;
            exit(1);
          }
        }
      }
      inFile.close();
      return make_tuple(mtlDict, textureFilename);
    }

    vector<GObject> loadOBJpass1(string filename) {
      // Declarations of stuff we want to parse from (string) input streams
      string mtlLibFileName, textureFilename, objName, objMtlName;
      materialDict mtlDict;
      float a, b, c;
      vec3 vertex;
      vector<vec3> vertices;
      vectorTexturePoint> textureCoords;
      vector<GObject> gobjects;
      //gobjects.push_back();
      int vIndex = 1;
      int vtIndex = 1;

      // File IO
      ifstream inFile;
      inFile.open(filename);
      if (inFile.fail()) {
        cout << "File not found." << endl;
        exit(1);
      }
      string lineString, linePrefix;

      while (getline(inFile, lineString)) {
        if (emptyOrCommentLine(lineString)) continue;
        istringstream lineStream(lineString);
        lineStream >> linePrefix;
        if (linePrefix == "mtllib") {
          lineStream >> mtlLibFileName;
          tie(mtlDict, textureFilename) = loadMTL(mtlLibFileName);
        }
        else if (linePrefix == "v") {
          lineStream >> a >> b >> c;
          vertex[0] = a;
          vertex[1] = b;
          vertex[2] = c;
          vertices.push_back(vertex);
        }
        else if (linePrefix == "o") {
          lineStream >> objName;
          skipToNextLine(inFile);
          lineStream >> linePrefix;
          if (linePrefix == "usemtl") {
            lineStream >> objMtlName;
            mtlDict.insert({mtlName, Colour(mtlName, round(255*r), round(255*g), round(255*b)});
          }
          else {
            cout << "Line after 'o' line should be 'usemtl materialName'." << endl;
            exit(1);
          }
        }
      }

      inFile.close();

      return gobjects;
    }

    float maxComponent(glm::vec3 v) {
      float greatest = std::max(std::max(v[0], v[1]), v[2]);
      return greatest;
    }

    // potentially most negative
    float minComponent(glm::vec3 v) {
      float smallest = std::min(std::min(v[0], v[1]), v[2]);
      return smallest;
    }

    std::vector<GObject> scale(int WIDTH, std::vector<GObject> gobjects) {
      float currentMinComponent = std::numeric_limits<float>::infinity();
      for (uint j=0; j<gobjects.size(); j++) {
        for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
          for (int k=0; k<3; k++) {
            float smallest = minComponent(gobjects.at(j).faces.at(i).vertices[k]);
            if (smallest < currentMinComponent) currentMinComponent = smallest;
          }
        }
      }

      float addFactor = currentMinComponent < 0.0f ? abs(currentMinComponent) : 0.0f;

      for (uint j=0; j<gobjects.size(); j++) {
        //std::cout << "gobject " << gobjects.at(j).name << '\n';
        for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
          for (int k=0; k<3; k++) {
            glm::vec3 v = gobjects.at(j).faces.at(i).vertices[k];
            v[0] += addFactor;
            v[1] += addFactor;
            v[2] += addFactor;
            gobjects.at(j).faces.at(i).vertices[k] = v;
            // std::cout << "NEW VERTEX: (" << v.x << ", " << v.y << ", " << v.z << ")\n";
          }
        }
      }

      float currentMaxComponent = -std::numeric_limits<float>::infinity();
      for (uint j=0; j<gobjects.size(); j++) {
        for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
          for (int k=0; k<3; k++) {
            float greatest = maxComponent(gobjects.at(j).faces.at(i).vertices[k]);
            if (greatest > currentMaxComponent) currentMaxComponent = greatest;
          }
        }
      }

      float multFactor = WIDTH / (currentMaxComponent);

      std::cout << "MULTIPLICATIVE SCALE FACTOR: " << multFactor << '\n';
      std::cout << "ADDITIVE SCALE FACTOR: " << addFactor << '\n';

      for (uint j=0; j<gobjects.size(); j++) {
        //std::cout << "gobject " << gobjects.at(j).name << '\n';
        for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
          for (int k=0; k<3; k++) {
            glm::vec3 v = gobjects.at(j).faces.at(i).vertices[k];
            v[0] *= multFactor;
            v[1] *= multFactor;
            v[2] *= multFactor;
            gobjects.at(j).faces.at(i).vertices[k] = v;
            //std::cout << "NEW VERTEX: (" << v.x << ", " << v.y << ", " << v.z << ")\n";
          }
        }
      }
      //std::cout << "finished scaling" << '\n';

      return gobjects;
    }
};
