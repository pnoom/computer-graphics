
#include <unordered_map>
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
    // FaceData just contain indices. This vector may not be necessary
    vector<faceData> allFaces;
    // These map object names to indices into the above vectors
    indexDict vertexDict, textureVertexDict;
    unordered_map<string, faceData> faceDict;

    OBJ_Structure () {}

    vector<GObject> toGObjects() {
      vector<GObject> result;
      // TODO: implement me

      // Loop through faceDict's key (i.e. all the objects in the scene, incl.
      // "loose") and build a TextureTriangle by referencing the array,
      // and then a ModelTriangle using the other array, and then make a gobject
      // and add it to the vector.

      string objName;
      faceData face;
      vec3_int vindices;
      vec3_int tindices;
      //vec3_int nindices;
      ModelTriangle vtriangle;
      TextureTriangle ttriangle;
      GObject gobject;

      // This loop won't work: need an ORDERED map, probably...
      for (auto pair=faceDict.begin(); pair != faceDict.end(); pair++) {
        objName = pair->first;
        face = pair->second;
        vindices = face<0>;
        // If we have texture vertices, make them into a TextureTriangle
        if (face<1>) {
          tindices = face<1>.value();
          ttriangle = TextureTriangle(textureFilename,
            allTextureVertices.at(tindices[0]),
            allTextureVertices.at(tindices[1]),
            allTextureVertices.at(tindices[2]));
        }
        vtriangle = ModelTriangle(
          allVertices.at(vindices[0]),
          allVertices.at(vindices[1]),
          allVertices.at(vindices[2]),
        ); // colour??? Need an object-material map!!!

        // This needs a VECTOR of ModelTriangles, FFS
        gobject = GObject(objName, ?, );
      }
      return result;
    }
};
