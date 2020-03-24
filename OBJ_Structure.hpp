
#include <unordered_map>
#include <map>
#include <tuple>
#include <optional>

using namespace std;
using namespace glm;

// Maps material name (e.g. White) to Colour
typedef unordered_map<string, Colour> materialDict;

// Maps from object name (e.g. light, back_wall) to indices into the
// vectors of vertices, texture vertices, and faces
typedef unordered_map<string, int> indexDict;

typedef array<int,3> vec3_int;
// typedef array<int,2> vec2_int;

// intermediate structure for storing the indices of the vertices and texture
// vertices that make up a face.
typedef tuple<vec3_int, optional<vec3_int>, optional<vec3_int>> faceData;


class OBJ_Structure {
  public:
    // We can only handle one mtl file per OBJ file atm.
    string mtlLibFileName;

    // These two fields are (maybe) populated by loadMTL
    string textureFilename;
    materialDict mtlDict;

    // The vectors that hold all the actual points
    vector<vec3> allVertices;
    vector<vec2> allTextureVertices;

    // These maps object names to indices into the above vectors
    map<string, faceData> faceDict;

    unordered_map<string, string> objMatNameDict;

    OBJ_Structure () {}

    vector<GObject> toGObjects() {
      vector<GObject> result;
      // TODO: implement me

      // Loop through faceDict's key (i.e. all the objects in the scene, incl.
      // "loose") and build a TextureTriangle by referencing the array,
      // and then a ModelTriangle using the other array, and then make a gobject
      // and add it to the vector.

      string objName = "dummy";
      faceData face;
      vec3_int vindices;
      vec3_int tindices;
      //vec3_int nindices;
      ModelTriangle vtriangle;
      TextureTriangle ttriangle;
      vector<ModelTriangle> triangles;

      // Assumes faceDict is an ORDERED map
      for (auto pair=faceDict.begin(); pair != faceDict.end(); pair++) {
        // If we've hit a new object, and the previous one was actually an
        // object whose Triangles we've created, then make a gobject from them
        if (!(objName == "dummy") && !(objName == pair->first)) {
          result.push_back(GObject(objName, mtlDict[objMatNameDict[objName]], triangles));
          triangles.clear();
        }
        objName = pair->first;
        face = pair->second;
        vindices = get<0>(face);
        vtriangle = ModelTriangle(
          allVertices.at(vindices[0]),
          allVertices.at(vindices[1]),
          allVertices.at(vindices[2]),
          mtlDict[objMatNameDict[objName]]); // colour
        // If we have texture vertices, make them into a TextureTriangle
        if (get<1>(face)) {
          tindices = get<1>(face).value();
          ttriangle = TextureTriangle(textureFilename,
            allTextureVertices.at(tindices[0]),
            allTextureVertices.at(tindices[1]),
            allTextureVertices.at(tindices[2]));
          vtriangle.maybeTextureTriangle.emplace(ttriangle);
        }
        // else set maybeTextureTriangle to nullopt? Or will it be that already?
      }
      return result;
    }
};

std::ostream& operator<<(std::ostream& os, const OBJ_Structure& structure)
{
    os << "mltLibFileName '" << structure.mtlLibFileName << "'" << endl;
    os << "textureFilename '" << structure.textureFilename << "'" << endl;
    for (auto pair=structure.mtlDict.begin(); pair != structure.mtlDict.end(); pair++) {
     cout << "mtlDict key '" << pair->first << "' value '" << pair->second << "'" << endl;
    }
    os << "allVertices.size() " << structure.allVertices.size() << endl;
    os << "allTextureVertices.size() " << structure.allTextureVertices.size() << endl;
    for (auto pair=structure.objMatNameDict.begin(); pair != structure.objMatNameDict.end(); pair++) {
     cout << "objMatNameDict key '" << pair->first << "' value '" << pair->second << "'" << endl;
    }
    os << "faceDict.size() " << structure.faceDict.size() << endl;
    // How to print tuples and optionals?
    // for (auto pair=structure.faceDict.begin(); pair != structure.faceDict.end(); pair++) {
    //  cout << "faceDict key '" << pair->first << "' value '" << pair->second << "'" << endl;
    // }
    return os;
}
