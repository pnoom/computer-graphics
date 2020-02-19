#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>

using namespace std;
using namespace glm;

class OBJ_IO {
  public:
    OBJ_IO () {}

    std::vector<GObject> loadOBJ(string filename) {
      return loadOBJpass1(filename);
    }

  private:
    bool isemptyline(char* line) {
      return line[0] == '\n';
    }

    unordered_map<string, Colour> loadMTL(string filename) {
      FILE* f = fopen(filename.c_str(), "r");
      int bufsize = 200;
      char buf[bufsize];
      std::string* tokens;
      unordered_map<string, Colour> materials;

      if (f == NULL) {
        std::cout << "Could not open file." << '\n';
        exit(1);
      }

      string matName;
      float rgb[3];

      while (!feof(f)) {
        fgets(buf, bufsize, f);
        if (isemptyline(buf)) continue;
        tokens = split(buf, ' ');
        //std::cout << "tokens[0]" << tokens[0] << '\n';
        if (tokens[0] == "newmtl") {
          matName = tokens[1];
          fgets(buf, bufsize, f);
          tokens = split(buf, ' ');
          if (tokens[0] != "Kd") {
            std::cout << "Line after 'newmtl' should be 'Kd r g b'" << '\n';
            exit(1);
          }
          else {
            size_t stof_thing;
            for (int i=0; i<3; i++) {
              rgb[i] = stof(tokens[i+1], &stof_thing);
            }
            Colour col(matName, 255*round(rgb[0]), 255*round(rgb[1]), 255*round(rgb[2]));
            materials[matName] = col;
          }
        }
      }
      fclose(f);
      return materials;
    }

    std::vector<GObject> loadOBJpass2(string filename,
    				  unordered_map<string, Colour> mtls,
    					 std::vector<glm::vec3> vertices,
    					 unordered_map<string, std::vector<glm::uint>> objects,
    					 unordered_map<string, string> object_mtl_map) {
      FILE* f = fopen(filename.c_str(), "r");
      int bufsize = 200;
      char buf[bufsize];
      std::string* tokens;
      std::vector<ModelTriangle> faces;
      std::vector<GObject> result;
      Colour colour;
      std::string object_name;

      if (f == NULL) {
        std::cout << "Could not open file." << '\n';
        exit(1);
      }

      //std::cout << "Starting 2nd pass" << '\n';

      while (!feof(f)) {
        fgets(buf, bufsize, f);
        if (isemptyline(buf)) continue;
        tokens = split(buf, ' ');

        if (tokens[0] == "o") {
          object_name = tokens[1];
          object_name.erase(object_name.end() - 1, object_name.end());

          colour = mtls[object_mtl_map[object_name]];

          while(!feof(f) && !isemptyline(buf)) {
    	if (tokens[0] == "f") {
    	  uint vindices[3];
    	  for (int i=1; i<=3; i++) {
    	    sscanf(tokens[i].c_str(), "%d/", &vindices[i-1]);
    	  }
    	  faces.push_back(ModelTriangle(vertices.at(vindices[0]-1),
    					vertices.at(vindices[1]-1),
    					vertices.at(vindices[2]-1), colour));
    	}
    	fgets(buf, bufsize, f);
            tokens = split(buf, ' ');
          }
          result.push_back(GObject(object_name, colour, faces));
          //std::cout << "faces.size() " << faces.size() << '\n';
          faces.clear();

        }
      }
      fclose(f);
      //std::cout << "result.size() " << result.size() << '\n';
      return result;
    }

    std::vector<GObject> loadOBJpass1(string filename) {
      FILE* f = fopen(filename.c_str(), "r");
      int bufsize = 200;
      char buf[bufsize];
      std::string* tokens;

      if (f == NULL) {
        std::cout << "Could not open file." << '\n';
        exit(1);
      }

      unordered_map<string, Colour> mtls;

      std::vector<glm::vec3> vertices;
      unordered_map<string, std::vector<glm::uint>> objects;
      unordered_map<string, string> object_mtl_map;

      string object_name, mtl_name;
      while (!feof(f)) {
        fgets(buf, bufsize, f);
        if (isemptyline(buf)) continue;
        tokens = split(buf, ' ');

        if (tokens[0] == "mtllib") {
          string mtllib_name = tokens[1];
          mtllib_name.erase(mtllib_name.end() - 1, mtllib_name.end());
          mtls = loadMTL(mtllib_name);
        }
        else if (tokens[0] == "o") {
          object_name = tokens[1];
          object_name.erase(object_name.end() - 1, object_name.end());
          std::cout << "o: " << object_name << '\n';
          objects[object_name] = std::vector<glm::uint>();

          fgets(buf, bufsize, f);
          tokens = split(buf, ' ');

          if (tokens[0] != "usemtl") {
            //std::cout << "Object " << object_name << " has no associated material." << '\n';
            exit(1);
          }

          mtl_name = tokens[1];
          mtl_name.erase(mtl_name.end() - 1, mtl_name.end());
          object_mtl_map[object_name] = mtl_name;

          fgets(buf, bufsize, f);
          tokens = split(buf, ' ');

          while(!feof(f) && !isemptyline(buf)) {
    	if (tokens[0] == "v") {
    	  float vec3_parts[3];
    	  size_t stof_thing;
    	  for (int i = 0; i < 3; i++) {
    	    vec3_parts[i] = stof(tokens[i+1], &stof_thing);
    	  }
    	  glm::vec3 v(vec3_parts[0], vec3_parts[1], vec3_parts[2]);
    	  std::cout << "vertex: " << v[0] << " " << v[1] << " " << v[2] << '\n';
    	  vertices.push_back(v);
    	  objects[object_name].push_back(vertices.size());
    	}
    	  fgets(buf, bufsize, f);
    	  tokens = split(buf, ' ');
          }
          //std::cout << "Object " << object_name << " has " << objects[object_name].size() << " vertices, with material " << object_mtl_map[object_name] << "\n";
        }
      }
      fclose(f);

      //std::cout << "First pass complete" << "\n";

      //TODO: vertices and faces can come in any order; this code is too strict on ordering!
      //      -> move the while(tokens[0] == "v") {...} logic out of following an object or smth
      return loadOBJpass2(filename, mtls, vertices, objects, object_mtl_map);
    }
};
