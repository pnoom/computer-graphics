
#include <unordered_map>
#include <tuple>

// Maps material name (e.g. White) to Colour
typedef unordered_map<string, Colour> materialDict;

// Maps from object name (e.g. light, back_wall) to indices into the
// vectors of vertices, texture vertices, and faces
typedef unordered_map<string, int> indexDict;

// intermediate structure for storing the indices of the vertices and texture
// vertices that make up a face.
typedef tuple<vec3<int>, vec3<int>> faceData;


class OBJ_Structure {
  // We can only handle one mtl file per OBJ file atm.
  string mtlLibFileName;

  // These are just for use during IO
  string currentObjName, currentObjMtlName;

  // These two fields are (maybe) populated by loadMTL
  string textureFilename;
  materialDict mtlDict;

  // The vectors that hold all the actual points
  vector<vec3> allVertices;
  vector<vec2> allTextureVertices;
  // These just contain indices
  vector<faceData> allFaces;
  // These map object names to indices into the above vectors
  indexDict vertexDict, textureVertexDict, faceDict;

  public:
    OBJ_Structure () {}
};
