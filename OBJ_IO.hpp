#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <optional>
#include "OBJ_Structure.hpp"
#include "Texture.hpp"

using namespace std;
using namespace glm;

class OBJ_IO {
  public:
    OBJ_IO () {}

    // For clarity: each ModelTriangle (a triangle in 3D space) may get their
    // "filling" from a TextureTriangle (a triangle in 2D space).

    tuple<vector<GObject>, optional<Texture>> loadOBJ(string filename) {
      optional<Texture> maybeTexture;
      OBJ_Structure structure = loadOBJpass1(filename);
      if (structure.textureFilename.empty())
        maybeTexture = nullopt;
      else maybeTexture.emplace(Texture(structure.textureFilename));
      //cout << structure << endl;
      return make_tuple(structure.toGObjects(), maybeTexture);
    }

    std::vector<GObject> scale_additive(std::vector<GObject> gobjects) {
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
      return gobjects;
    }

    std::vector<GObject> scale_multiplicative(int width, std::vector<GObject> gobjects) {
      float currentMaxComponent = -std::numeric_limits<float>::infinity();
      for (uint j=0; j<gobjects.size(); j++) {
        for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
          for (int k=0; k<3; k++) {
            float greatest = maxComponent(gobjects.at(j).faces.at(i).vertices[k]);
            if (greatest > currentMaxComponent) currentMaxComponent = greatest;
          }
        }
      }

      float multFactor = width / (currentMaxComponent);

      //std::cout << "MULTIPLICATIVE SCALE FACTOR: " << multFactor << '\n';
      //std::cout << "ADDITIVE SCALE FACTOR: " << addFactor << '\n';

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

  private:
    void skipToNextLine(ifstream& inFile) {
      inFile.ignore(numeric_limits<int>::max(), '\n');
    }

    bool emptyOrCommentLine(string lineString) {
      return (lineString.empty() || lineString.front() == '#');
    }

    faceData processFaceLine(istringstream& lineStream) {
      string faceTerm;
      int numRead;
      vec3_int vindices;
      vec3_int tindices;
      // vec3_int nindices;
      bool vts = true;
      faceData face;
      int i = 0;

      // Repeatedly get "v1/vt1/vn1" or similar, then parse /-delimited ints.
      // Ignore vector normals for now.
      while (lineStream >> faceTerm) {
        numRead = sscanf(faceTerm.c_str(), "%d/%d", &vindices[i], &tindices[i]); // nindices[i]
        if (numRead == 1) vts = false;
        i += 1;
      }
      // Don't forget to subtract one from all the indices, since OBJ files use
      // 1-based indices
      for (int i=0; i<(int)vindices.size(); i++) vindices[i] -= 1;
      if (vts) {
        for (int i=0; i<(int)tindices.size(); i++) tindices[i] -= 1;
        face = make_tuple(vindices, tindices, nullopt);
      }
      else {
        face = make_tuple(vindices, nullopt, nullopt);
      }
      return face;
    }

    // Don't bother having an MTL_Structure class, just use a tuple
    tuple<materialDict, string> loadMTL(string filename) {
      // Return values
      materialDict mtlDict;
      string textureFilename;

      // Temp vars
      string mtlName;
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
        }
        // Assumes a "newmtl" line has come before, initialising mtlName
        else if (linePrefix == "Kd") {
          lineStream >> r >> g >> b;
          mtlDict.insert({mtlName, Colour(mtlName, round(255*r), round(255*g), round(255*b))});
        }
      }
      inFile.close();
      return make_tuple(mtlDict, textureFilename);
    }

    OBJ_Structure loadOBJpass1(string filename) {
      // Store all intermediate stuff in here. Use it to build a vector of
      // gobjects.
      OBJ_Structure structure;

      // Temp vars
      float a, b, c;
      vec3 vertex;
      vec2 textureVertex;
      faceData face;
      string currentObjName = "loose";
      string currentObjMtlName;

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
        //cout << "linePrefix '" << linePrefix << "'" << endl;
        if (linePrefix == "mtllib") {
          // cout << "This should only appear once" << endl;
          lineStream >> structure.mtlLibFileName;
          // TODO: check which of these is empty, if any, and do sth appropriate
          tie(structure.mtlDict, structure.textureFilename) = loadMTL(structure.mtlLibFileName);
        }
        else if (linePrefix == "v") {
          lineStream >> a >> b >> c;
          vertex[0] = a;
          vertex[1] = b;
          vertex[2] = c;
          structure.allVertices.push_back(vertex);
        }
        else if (linePrefix == "vt") {
          lineStream >> a >> b;
          textureVertex[0] = a;
          textureVertex[1] = b;
          structure.allTextureVertices.push_back(textureVertex);
        }
        else if (linePrefix == "f") {
          face = processFaceLine(lineStream);
          structure.faceDict.insert({currentObjName, face});
        }
        else if (linePrefix == "o") {
          lineStream >> currentObjName;
        }
        // Assumes an "o" line has come before it, initialising currentObjName
        else if (linePrefix == "usemtl") {
            lineStream >> currentObjMtlName;
            structure.objMatNameDict.insert({currentObjName, currentObjMtlName});
        }
      }
      inFile.close();
      return structure;
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
};
