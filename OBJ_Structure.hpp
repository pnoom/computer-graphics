
#include <unordered_map>
#include <tuple>

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
typedef tuple<vec3_int, vec3_int> faceData;


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
    indexDict vertexDict, textureVertexDict, faceDict;

    OBJ_Structure () {}
};
