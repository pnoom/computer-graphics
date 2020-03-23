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

using namespace std;
using namespace glm;

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
      vector<GObject> res;
      OBJ_Structure structure = loadOBJpass1(filename);
      res = structure.toGObjects();
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

    // Don't bother having an MTL_Structure class, just use a tuple
    tuple<materialDict, string> loadMTL(string filename) {
      cout << "Loading included mtl library..." << endl;
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
      cout << "textureFilename '" << textureFilename << "'" << endl;
      //cout << "mtlDict" << mtlDict << endl;
      //for (auto pair=mtlDict.begin(); pair != mtlDict.end(); pair++) {
      //  cout << "mtlDict: key '" << pair->first << "' value '" << pair->second << "'" << endl;
      //}
      cout << "Loaded mtl library." << endl;
      return make_tuple(mtlDict, textureFilename);
    }

    OBJ_Structure loadOBJpass1(string filename) {
      // Store all intermediate stuff in here. Use it to build a vector of
      // gobjects.
      OBJ_Structure structure;

      // Temp vars
      float a, b, c;
      int d, e;
      vec3 vertex;
      vec2 textureVertex;
      faceData face;
      int vIndex = 1;
      int vtIndex = 1;
      string currentObjName = "loose";
      string currentObjMtlName;
      vec3_int vindices;
      vec3_int tindices;
      string faceTerm; // "v1/" or "v1/vt1" (TODO: or "v1/vt1/vn1", or "v1//vn1")
      std::string* tokens; // e.g. "v1" and "vt1" from "v1/vt1"

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
          structure.vertexDict.insert({currentObjName, vIndex});
          vIndex += 1;
        }
        else if (linePrefix == "vt") {
          lineStream >> a >> b;
          textureVertex[0] = a;
          textureVertex[1] = b;
          structure.allTextureVertices.push_back(textureVertex);
          structure.textureVertexDict.insert({currentObjName, vtIndex});
          vtIndex += 1;
        }
        else if (linePrefix == "f") {
          // TODO: deal with the / delimiter and optional texture vertex logic
          for (int i=0; i<3; i++) {
            lineStream >> faceTerm;
            tokens = split(faceTerm, ',');
            //d = stoi(tokens[0], NULL);
            //e = stoi(tokens[1], NULL);
            //vindices[0] = d;
            //tindices[0] = e;
            //lineStream >> vindices[0] >> vindices[1] >> vindices[2];
          }
          //face = make_tuple(vindices, tindices); // this should copy these arrays from the temp variables

          // Do I actually need this?
          //structure.allFaces.push_back(face);

          //structure.faceDict.insert({currentObjName, face});
        }
        else if (linePrefix == "o") {
          lineStream >> currentObjName;
        }
        // Assumes an "o" line has come before it, initialising currentObjName
        else if (linePrefix == "usemtl") {
            lineStream >> currentObjMtlName;
            // At some point, get material from dict like this:
            // structure.mtlDict[currentObjMtlName];
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
