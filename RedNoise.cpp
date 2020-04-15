#include <ModelTriangle.h>
#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <RayTriangleIntersection.h>
#include <Utils.h>
#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <math.h>
#include <filesystem>
#include <tuple>
#include <optional>

#include "Texture.hpp"
#include "GObject.hpp"
#include "OBJ_IO.hpp"
#include "Camera.hpp"
#include "DepthBuffer.hpp"
#include "Light.hpp"

using namespace std;
using namespace glm;

namespace fs = std::filesystem;

// Definitions
// ---
#define WIDTH 640
#define HEIGHT 480

#define SCREENSHOT_DIR "./screenies/"
#define SCREENSHOT_SUFFIX ".ppm"

fs::path screenshotDir;

Colour COLOURS[] = {Colour(255, 0, 0), Colour(0, 255, 0), Colour(0, 0, 255)};
Colour WHITE = Colour(255, 255, 255);
Colour BLACK = Colour(0, 0, 0);

typedef enum {WIRE, RASTER, RAY} View_mode;
typedef enum {WINDOW, TEXTURE} Draw_buf;
// Global Object Declarations
// ---

OBJ_IO obj_io;
std::vector<GObject> gobjects;
DrawingWindow window;

uint32_t *texture_buffer;

std::vector<Texture> textures;
//Texture logoTexture;
//Texture jamdyTexture;

DepthBuffer depthbuf;
Camera camera;
View_mode current_mode;
Draw_buf buf_mode;
Light light;
bool animating = false;

int number_of_AA_samples = 1;
int frame_no = 0;
// Simple Helper Functions
// ---
float max(float A, float B) { if (A > B) return A; return B; }
float min(float A, float B) { if (A < B) return A; return B; }
int modulo(int x, int y) { if (y == 0) return x; return ((x % y) + x) % y; }
bool comparator(CanvasPoint p1, CanvasPoint p2) { return (p1.y < p2.y); }
void printVec3(vec3 v) { cout << "(" << v.x << ", " << v.y << ", " << v.z << ")\n"; }
bool isLight(GObject gobj) { return (gobj.name == "light"); }
glm::mat3 rotMatX(float angle) { return mat3(1,0,0, 0,cos(angle),-sin(angle), 0,sin(angle),cos(angle)); }
glm::mat3 rotMatY(float angle) { return mat3(cos(angle),0,sin(angle), 0,1,0, -sin(angle),0,cos(angle)); }
glm::mat3 rotMatZ(float angle) { return mat3(cos(angle),-sin(angle),0, sin(angle),cos(angle),0, 0,0,1); }
float deg2rad(float deg) { return (deg * M_PI) / 180; }

void printMat3(mat3 m) {
  printVec3(m[0]);
  printVec3(m[1]);
  printVec3(m[2]);
}

uint32_t get_rgb(Colour colour) { return (0 << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue; }
uint32_t get_rgb(vec3 rgb) { return (uint32_t)((255<<24) + (int(rgb[0])<<16) + (int(rgb[1])<<8) + int(rgb[2])); }

// Adapted from:
// https://stackoverflow.com/questions/5438482/getting-the-current-time-as-a-yyyy-mm-dd-hh-mm-ss-string/5438502
string getDateTimeString() {
  char buf[80];
  time_t epochTime;
  time(&epochTime);
  tm* locTime = localtime(&epochTime);
  strftime(buf, 79, "%Y-%m-%d_%H:%M:%S", locTime);
  return string(buf);
}

void writePPM() {
  std::string frame = std::string(5 - to_string(frame_no).length(), '0') + to_string(frame_no);
  fs::path screenshotName = fs::path(frame + SCREENSHOT_SUFFIX);
  fs::path outputFile = screenshotDir / screenshotName;

  FILE* f = fopen(string(outputFile).c_str(), "w");
  if (f == NULL) {
    std::cout << "Could not open file." << '\n';
    exit(1);
  }
  fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT); // P6

  uint32_t colour;
  uint8_t r,g,b;
  for (int j=0; j<HEIGHT; j++) {
    for (int i=0; i<WIDTH; i++) {
      colour = window.getPixelColour(i, j);
      r = (uint8_t)((colour >> 16) & 0xff);
      g = (uint8_t)((colour >> 8) & 0xff);
      b = (uint8_t)(colour & 0xff);
      fputc(r, f);
      fputc(g, f);
      fputc(b, f);
    }
  }
  fclose(f);
}

// For some reason, all the ways to "append" or "copy" vectors in C++ yield an
// iterator, not a vector, which is no good. So we have to roll our own,
// inefficient verson.
vector<GObject> joinGObjectVectors(vector<GObject> gobjects1, vector<GObject> gobjects2) {
  vector<GObject> result;
  for (auto g=gobjects1.begin(); g != gobjects1.end(); g++) {
    result.push_back(*g);
  }
  for (auto g=gobjects2.begin(); g != gobjects2.end(); g++) {
    result.push_back(*g);
  }
  return result;
}

vec3 averageVerticesOfFaces(vector<ModelTriangle> faces) {
  vec3 accVerts = vec3(0.0f, 0.0f, 0.0f);
  int numVerts = 0;
  for (auto f=faces.begin(); f != faces.end(); f++) {
    for (auto i=0; i<3; i++) {
      accVerts += (*f).vertices[i];
      numVerts += 1;
    }
  }
  return accVerts / (float)numVerts;
}

GObject& getGObjectByName(string gobjectName) {
  for (auto g=gobjects.begin(); g != gobjects.end(); g++) {
    if ((*g).name == gobjectName) return (*g);
  }
  cout << "Couldn't find a gobject called '" << gobjectName << "'." << endl;
  exit(1);
}

vec3 getCentreOf(string gobjectName) {
  for (auto g=gobjects.begin(); g != gobjects.end(); g++) {
    if ((*g).name == gobjectName) return averageVerticesOfFaces((*g).faces);
  }
  cout << "Couldn't find a gobject called '" << gobjectName << "'." << endl;
  return vec3(0.0f,0.0f,0.0f);
}

void translateGObject(vec3 translationVector, GObject &gobject) {
  for (uint i = 0; i < gobject.faces.size(); i++) {
    for (uint j = 0; j < 3; j++) {
      gobject.faces.at(i).vertices[j].x += translationVector.x;
      gobject.faces.at(i).vertices[j].y += translationVector.y;
      gobject.faces.at(i).vertices[j].z += translationVector.z;
    }
  }
}

void translateGObjectToOrigin(GObject &gobject) {
  vec3 objCentre = averageVerticesOfFaces(gobject.faces);
  translateGObject(-objCentre, gobject);
}

void rotateGObjectAboutY(float deg, GObject &gobject) {
  mat3 transform = rotMatY(deg2rad(deg));
  //printMat3(transform);
  for (uint i = 0; i < gobject.faces.size(); i++) {
    for (uint j = 0; j < 3; j++) {
      gobject.faces.at(i).vertices[j] = transform * gobject.faces.at(i).vertices[j];
    }
  }
}

void rotateGObjectAboutYInPlace(float deg, GObject &gobject) {
  vec3 objCentre = averageVerticesOfFaces(gobject.faces);
  translateGObject(-objCentre, gobject);
  rotateGObjectAboutY(deg, gobject);
  translateGObject(objCentre, gobject);
}

void rotateTeaPot(float deg) {
  rotateGObjectAboutYInPlace(deg, getGObjectByName("teapot"));
}

void readOBJs() {
  vector<GObject> scene;
  vector<GObject> logo;
  vector<GObject> teapot;

  // This tuple/optional nonsense is just to avoid changing OBJ_IO/Structure too much.
  // For each obj file we load, it may have a texture file. We should have a global
  // variable for each texture that we know will actually exist and set it to the
  // value of the optional here.
  optional<Texture> maybeTexture;

  tie(scene, maybeTexture) = obj_io.loadOBJ("jamdy.obj");
  scene = obj_io.scale_additive(scene);
  //scene = obj_io.scale_multiplicative(WIDTH, scene);
  if (maybeTexture) textures.push_back(maybeTexture.value());

  tie(logo, maybeTexture) = obj_io.loadOBJ("logo.obj");
  logo = obj_io.scale_additive(logo);
  logo = obj_io.scale_multiplicative(1000, logo);
  if (maybeTexture) textures.push_back(maybeTexture.value());

  tie(teapot, maybeTexture) = obj_io.loadOBJ("teapot200.obj");
  teapot = obj_io.scale_additive(teapot);
  teapot = obj_io.scale_multiplicative(WIDTH, teapot);
  if (maybeTexture) textures.push_back(maybeTexture.value());

  gobjects = joinGObjectVectors(scene, logo);
  gobjects = joinGObjectVectors(gobjects, teapot);

  // Find the light gobject, average its vertices, and use that as the light pos
  // (but shift it down slightly first, so it doesn't lie exactly within the
  // light object).
  // Otherwise the light keeps its default position.
  vector<GObject>::iterator maybeLight = find_if(gobjects.begin(), gobjects.end(), isLight);
  if (maybeLight != gobjects.end()) {
    light.Position = averageVerticesOfFaces((*maybeLight).faces) - vec3(0.0f, 10.0f, 0.0f);
  }
}

// Raster Functions
// ---
std::vector<double> interpolate(double from, double to, int numberOfValues) {
  double delta = (to - from) / (numberOfValues - 1);
  std::vector<double> v;
  for (int i = 0; i < numberOfValues; i++) v.push_back(from + (i * delta));
  return v;
}

std::vector<CanvasPoint> interpolate_line(CanvasPoint from, CanvasPoint to) {
  float delta_X    = to.x - from.x;
  float delta_Y    = to.y - from.y;
  float delta_tp_X = to.texturePoint.x - from.texturePoint.x;
  float delta_tp_Y = to.texturePoint.y - from.texturePoint.y;
  double delta_dep = to.depth - from.depth;

  float no_steps = max(abs(delta_X), abs(delta_Y));

  float stepSize_X    = delta_X / no_steps;
  float stepSize_Y    = delta_Y / no_steps;
  float stepSize_tp_X = delta_tp_X / no_steps;
  float stepSize_tp_Y = delta_tp_Y / no_steps;
  double stepSize_dep = delta_dep / no_steps;

  std::vector<CanvasPoint> v;
  for (float i = 0.0; i < no_steps; i++) {
    CanvasPoint interp_point(from.x + (i * stepSize_X), from.y + (i * stepSize_Y), from.depth + (i * stepSize_dep));
    TexturePoint interp_tp(from.texturePoint.x + (i * stepSize_tp_X), from.texturePoint.y + (i * stepSize_tp_Y), to.texturePoint.textureName);
    interp_point.texturePoint = interp_tp;
    v.push_back(interp_point);
  }
  return v;
}

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour) {
  if (buf_mode == TEXTURE) return;
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    if (depthbuf.update(pixel)) window.setPixelColour(round(x), round(y), get_rgb(colour));
  }
}

uint32_t get_textured_pixel(TexturePoint texturePoint) {
  //std::cout << "Texture name: " << texturePoint.textureName << '\n';
  for (uint i = 0; i < textures.size(); i++) {
    if (textures.at(i).textureFilename == texturePoint.textureName)
      return textures.at(i).ppm_image[(int)(round(texturePoint.x) + (round(texturePoint.y) * textures.at(i).width))];
  }
  return ((255 << 16) + 255);
  //return textures.at(0).ppm_image[(int)(round(texturePoint.x) + (round(texturePoint.y) * textures.at(0).width))];
}

void drawTexturedLine(CanvasPoint P1, CanvasPoint P2, CanvasTriangle triangle) {
  if (triangle.colour.name == "material_0") {
    P1.texturePoint.x = 0.0f;
    P2.texturePoint.x = 64.0f;
  }

  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    uint32_t colour = get_textured_pixel(pixel.texturePoint);
    if (round(x) >= 0 && round(x) < WIDTH && round(y) >= 0 && round(y) < HEIGHT) {
      if (depthbuf.update(pixel)) {
        if (buf_mode == WINDOW) {
          window.setPixelColour(round(x), round(y), colour);
        }
        else {
          texture_buffer[(int)(round(x) + (WIDTH * round(y)))] = colour;
        }
      }
    }
    //uint32_t colour = 0;
  }
}

void drawStrokedTriangle(CanvasTriangle triangle) {
  drawLine(triangle.vertices[0], triangle.vertices[1], triangle.colour);
  drawLine(triangle.vertices[0], triangle.vertices[2], triangle.colour);
  drawLine(triangle.vertices[1], triangle.vertices[2], triangle.colour);
}

void sortTrianglePoints(CanvasTriangle *triangle) {
  std::sort(std::begin(triangle->vertices), std::end(triangle->vertices), comparator);
}

void equateTriangles(CanvasTriangle from, CanvasTriangle *to) {
  for (int i = 0; i < 3; i++) {
    to->vertices[i].x = from.vertices[i].x;
    to->vertices[i].y = from.vertices[i].y;
    to->vertices[i].depth = from.vertices[i].depth;
    to->vertices[i].texturePoint = from.vertices[i].texturePoint;
  }
  to->colour = from.colour;
  to->textured = from.textured;
}

bool compareTriangles(ModelTriangle A, ModelTriangle B) {
  for (int i = 0; i < 3; i++) {
    if (round(A.vertices[i][0]) != round(B.vertices[i][0])) return false;
    if (round(A.vertices[i][1]) != round(B.vertices[i][1])) return false;
    if (round(A.vertices[i][2]) != round(B.vertices[i][2])) return false;
  }
  // TODO compare colours too
  return true;
}

void getTopBottomTriangles(CanvasTriangle triangle, CanvasTriangle *top, CanvasTriangle *bottom) {
  if ((round(triangle.vertices[0].y) == round(triangle.vertices[1].y)) || (round(triangle.vertices[1].y) == round(triangle.vertices[2].y))) {
    CanvasPoint temp = triangle.vertices[1];
    triangle.vertices[1] = triangle.vertices[2];
    triangle.vertices[2] = temp;
    equateTriangles(triangle, top);
    equateTriangles(triangle, bottom);
    return;
  }

  CanvasPoint point4;
  std::vector<CanvasPoint> v = interpolate_line(triangle.vertices[0], triangle.vertices[2]);
  for (float i = 0.0; i < v.size(); i++) {
    if (round(v.at(i).y) == round(triangle.vertices[1].y)) {
      point4 = v.at(i);
    }
  }

  //CONSTRUCTOR: CanvasTriangle(CanvasPoint v0, CanvasPoint v1, CanvasPoint v2, Colour c)
  CanvasTriangle top_Triangle(triangle.vertices[0], point4, triangle.vertices[1], triangle.colour, triangle.textured);
  CanvasTriangle bot_Triangle(point4, triangle.vertices[2], triangle.vertices[1], triangle.colour, triangle.textured);

  equateTriangles(top_Triangle, top);
  equateTriangles(bot_Triangle, bottom);
}

void fillFlatBaseTriangle(CanvasTriangle triangle, int side1_A, int side1_B, int side2_A, int side2_B) {
  std::vector<CanvasPoint> side1 = interpolate_line(triangle.vertices[side1_A], triangle.vertices[side1_B]);
  std::vector<CanvasPoint> side2 = interpolate_line(triangle.vertices[side2_A], triangle.vertices[side2_B]);

  uint last_drawn_y = round(side1.at(0).y);
  // std::cout << "Point at (" << side1.at(0).x << ", " << side1.at(0).y << "), colour " << triangle.colour;
  // std::cout << "  has texturepoints " << side1.at(0).texturePoint << " textured = " << triangle.textured << endl;
  if (triangle.textured) drawTexturedLine(side1.at(0), side2.at(0), triangle);
  else drawLine(side1.at(0), side2.at(0), triangle.colour);

  for (uint i = 0; i < side1.size(); i++) {
    if (round(side1.at(i).y) != last_drawn_y) {
      int j = 0;
      while ((j < (int)side2.size() - 1) && (round(side2.at(j).y) <= last_drawn_y)) {
	       j++;
      }
      if (triangle.textured) drawTexturedLine(side1.at(i), side2.at(j), triangle);
      else {
        drawLine(side1.at(i), side2.at(j), triangle.colour);
      }
      last_drawn_y++;
    }
  }
}

bool pointsOnLine(CanvasTriangle triangle, int a, int b, int c) {
  std::vector<CanvasPoint> v = interpolate_line(triangle.vertices[a], triangle.vertices[b]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[c].x)) && (round(v.at(i).y) == round(triangle.vertices[c].y))) {
      return true;
    }
  }
  return false;
}

bool triangleIsLine(CanvasTriangle triangle) {
  if (pointsOnLine(triangle, 0, 1, 2)) return true;
  if (pointsOnLine(triangle, 0, 2, 1)) return true;
  if (pointsOnLine(triangle, 1, 2, 0)) return true;
  return false;
}

void drawFilledTriangle(CanvasTriangle triangle) {
  sortTrianglePoints(&triangle);
  CanvasTriangle triangles[2];

  if (triangleIsLine(triangle)) {
    drawLine(triangle.vertices[0], triangle.vertices[1], triangle.colour);
    drawLine(triangle.vertices[1], triangle.vertices[2], triangle.colour);
  }
  else {
    getTopBottomTriangles(triangle, &triangles[0], &triangles[1]);
    fillFlatBaseTriangle(triangles[0], 0, 1, 0, 2);
    fillFlatBaseTriangle(triangles[1], 0, 1, 2, 1);
  }
}

glm::vec3 getAdjustedVector(glm::vec3 v) {
  glm::vec3 cam2vertex = v - camera.position;
  return cam2vertex * camera.orientation;
}

CanvasPoint projectVertexInto2D(glm::vec3 v) {
  // Vector from camera to verted adjusted w.r.t. to the camera's orientation.
  glm::vec3 adjVec = getAdjustedVector(v);

  double w_i;                      //  width of canvaspoint from camera axis
  double h_i;                      // height of canvaspoint from camera axis
  double d_i = camera.focalLength; // distance from camera to axis extension of canvas

  double depth = sqrt((adjVec.z * adjVec.z) + (adjVec.x * adjVec.x) + (adjVec.y * adjVec.y));

  w_i = ((adjVec.x * d_i) / adjVec.z) + (WIDTH / 2);
  h_i = ((adjVec.y * d_i) / adjVec.z) + (HEIGHT / 2);

  CanvasPoint res((float)w_i, (float)h_i, depth);
  return res;
}

CanvasTriangle projectTriangleOntoImagePlane(ModelTriangle triangle) {
  CanvasTriangle result;

  Texture t;
  if (triangle.maybeTextureTriangle) {
    for (uint j = 0; j < textures.size(); j++) {
      if (triangle.maybeTextureTriangle.value().textureFilename == textures.at(j).textureFilename)
        t = textures.at(j);
    }
  }

  for (int i = 0; i < 3; i++) {
    result.vertices[i] = projectVertexInto2D(triangle.vertices[i]);
    if (triangle.maybeTextureTriangle) {

      result.vertices[i].texturePoint.x = triangle.maybeTextureTriangle.value().vertices[i][0];
      result.vertices[i].texturePoint.y = triangle.maybeTextureTriangle.value().vertices[i][1];
      result.vertices[i].texturePoint.x *= t.width;
      result.vertices[i].texturePoint.y *= t.height;
      result.vertices[i].texturePoint.textureName = triangle.maybeTextureTriangle.value().textureFilename;
    }
    else {
      result.vertices[i].texturePoint = TexturePoint(-1, -1, "UNASSIGNED");
    }
  }
  result.colour = triangle.colour;
  result.textured = false;
  if (triangle.maybeTextureTriangle) {
    result.textured = true;
  }
  return result;
}

// Raytracing Functions
// ---
RayTriangleIntersection getPossibleIntersection(ModelTriangle triangle, glm::vec3 rayDir, glm::vec3 point) {
  glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
  glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
  glm::vec3 SPVector = point - triangle.vertices[0];
  glm::mat3 DEMatrix(-rayDir, e0, e1);
  glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

  float t = possibleSolution[0];
  float u = possibleSolution[1];
  float v = possibleSolution[2];

  if (((0.0f <= u) && (u <= 1.0f)) &&
      ((0.0f <= v) && (v <= 1.0f)) &&
      (u + v <= 1.0f) && (t >= 0)) {
        glm::vec3 point3d = triangle.vertices[0] + ((u * e0) + (v * e1));
        RayTriangleIntersection res(point3d, t * glm::length(rayDir), triangle, true, u, v);
        return res;
  }

  return RayTriangleIntersection();
}

RayTriangleIntersection getClosestIntersection(glm::vec3 rayDir) {
  RayTriangleIntersection closestIntersectionFound = RayTriangleIntersection();

  for (uint j=0; j<gobjects.size(); j++) {
    for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
      ModelTriangle triangle = gobjects.at(j).faces.at(i);
      RayTriangleIntersection possibleSolution = getPossibleIntersection(triangle, rayDir, camera.position);

      if (possibleSolution.isSolution) {
        if (possibleSolution.distanceFromPoint < closestIntersectionFound.distanceFromPoint) {
          closestIntersectionFound = possibleSolution;
        }
      }
    }
  }
  //if (!closestIntersectionFound.isSolution) std::cout << "Fired ray did not collide with geometry." << '\n';
  return closestIntersectionFound;
}

float getAngleOfIncidence(glm::vec3 point, ModelTriangle triangle) {
  glm::vec3 norm_1 = glm::normalize(glm::cross((triangle.vertices[2] - triangle.vertices[0]), (triangle.vertices[1] - triangle.vertices[0])));
  glm::vec3 norm_2 = glm::normalize(glm::cross((triangle.vertices[1] - triangle.vertices[0]), (triangle.vertices[2] - triangle.vertices[0])));

  glm::vec3 point_to_light = -light.Position + point;
  point_to_light = glm::normalize(point_to_light);

  float AOI = glm::dot(norm_1, point_to_light);
  if ((AOI < 0.0f) || (AOI >= 1.0f)) {
    AOI = glm::dot(norm_2, point_to_light);
  }
  return AOI;
}

bool isPointInShadow(glm::vec3 point, ModelTriangle self) {
  glm::vec3 rayDir = -light.Position + point;
  for (uint j=0; j<gobjects.size(); j++) {
    for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
      ModelTriangle triangle = gobjects.at(j).faces.at(i);
      if (!compareTriangles(self, triangle)) {
        RayTriangleIntersection intersection = getPossibleIntersection(triangle, rayDir, light.Position);
        if (intersection.isSolution) {
          if (intersection.distanceFromPoint < glm::length(rayDir)) {
            return true;
          }
        }
      }

    }
  }
  return false;
}

Colour getTextureColourFromRasterizer(int i, int j) {
  uint32_t intCol = texture_buffer[i + (WIDTH * j)];
  uint8_t red = (intCol >> 16) & 0xff;
  uint8_t green = (intCol >> 8) & 0xff;
  uint8_t blue = (intCol) & 0xff;

  Colour res(red, green, blue);
  // cout << res << endl;
  return res;
}

Colour getAdjustedColour(RayTriangleIntersection intersection, int i, int j) {
  Colour inputColour = intersection.intersectedTriangle.colour;
  if (intersection.intersectedTriangle.maybeTextureTriangle)
    inputColour = getTextureColourFromRasterizer(i, j);

  float AOI = getAngleOfIncidence(intersection.intersectionPoint, intersection.intersectedTriangle);
  float intensity = light.getIntensityAtPoint(intersection.intersectionPoint);
  bool pointInShadow = isPointInShadow(intersection.intersectionPoint, intersection.intersectedTriangle);

  Colour res;
  Colour ambient(inputColour.name + " AMBIENT", inputColour.red/5, inputColour.green/5, inputColour.blue/5);
  if (pointInShadow) return ambient;
  float adjAOI = AOI + 0.3;
  if (adjAOI > 1) adjAOI = 1;
  float rgbFactor = intensity * adjAOI;
  if (rgbFactor > 1) rgbFactor = 1;

  res.red = round(min(255.0f, max(ambient.red, inputColour.red * rgbFactor)));
  res.green = round(min(255.0f, max(ambient.green, inputColour.green * rgbFactor)));
  res.blue = round(min(255.0f, max(ambient.blue, inputColour.blue * rgbFactor)));
  res.name = inputColour.name + " LIGHT ADJUSTED";

  return res;
}

// High Level Functions
// ---
void drawGeometryViaRayTracing() {
  mat3 adjOrientation(camera.orientation[0], -camera.orientation[1], camera.orientation[2]);

  for (int j = 0; j < HEIGHT; j++) {
    for (int i = 0; i < WIDTH; i++) {
      // Note: the sign of the y value here is flipped
      //    in pixelRay and adjOrientation
      //    to ensure continuity between raytracer and rasteriser

      int x =  i - WIDTH / 2;
      int y = -j + HEIGHT / 2;

      glm::vec3 subPixelRays[number_of_AA_samples];
      for (int sampleIndex = 0; sampleIndex < number_of_AA_samples; sampleIndex++) {
        //Sets up the x and y sub-pixel offsets using modulo and boundary conditions
        float x_Offset = 0.25f;
        float y_Offset = 0.25f;
        if (modulo(sampleIndex, (number_of_AA_samples / 2)) == 0) x_Offset *= -1;
        if (sampleIndex >= (number_of_AA_samples / 2)) y_Offset *= -1;
        if (sampleIndex >= 4) {
          y_Offset *= 0.5f;
          x_Offset *= 0.5f;
        }

        glm::vec3 pr = glm::vec3(x - x_Offset, y - y_Offset, camera.focalLength);
        subPixelRays[sampleIndex] = pr;
      }

      int AA_red = 0, AA_green = 0, AA_blue = 0;

      for (int sampleIndex = 0; sampleIndex < number_of_AA_samples; sampleIndex++) {
        subPixelRays[sampleIndex] = subPixelRays[sampleIndex] * adjOrientation;
        RayTriangleIntersection subPixel_RTI = getClosestIntersection(subPixelRays[sampleIndex]);

        if (subPixel_RTI.isSolution) {
          Colour adjustedColour = getAdjustedColour(subPixel_RTI, i, j);
          AA_red += adjustedColour.red;
          AA_green += adjustedColour.green;
          AA_blue += adjustedColour.blue;
        }
      }

      uint8_t avg_red = AA_red / number_of_AA_samples;
      uint8_t avg_green = AA_green / number_of_AA_samples;
      uint8_t avg_blue = AA_blue / number_of_AA_samples;
      uint32_t avg_colour = (avg_red << 16) + (avg_green << 8) + (avg_blue);
      window.setPixelColour(i, j, avg_colour);

      // Legacy code for simple no AA raytracer; remove?
      /*
      int x =  i - WIDTH / 2;
      int y = -j + HEIGHT / 2;
      glm::vec3 pixelRay(x, y, camera.focalLength);
      pixelRay = pixelRay * adjOrientation;

      RayTriangleIntersection RTI = getClosestIntersection(pixelRay);
      if (RTI.isSolution) {
        //std::cout << "Found solution with i: " << i << ", j: " << j << '\n';
        Colour adjustedColour = getAdjustedColour(RTI, i, j);
        window.setPixelColour(i, j, get_rgb(adjustedColour));
      }
      else window.setPixelColour(i, j, get_rgb(BLACK));
      */
    }
  }
}

void drawGeometry(bool filled) {
  for (uint i = 0; i < gobjects.size(); i++) {
    for (uint j = 0; j < gobjects.at(i).faces.size(); j++) {
      CanvasTriangle projectedTriangle = projectTriangleOntoImagePlane(gobjects.at(i).faces.at(j));
      if (filled) drawFilledTriangle(projectedTriangle);
      else drawStrokedTriangle(projectedTriangle);
    }
  }

  CanvasPoint lightCP = projectVertexInto2D(light.Position);
  if ((lightCP.x >= 0 && lightCP.x < WIDTH) && (lightCP.y >= 0 && lightCP.y < HEIGHT))
    window.setPixelColour(lightCP.x, lightCP.y, get_rgb(BLACK));
}

void clearScreen() {
  window.clearPixels();

  for(int y=0; y<window.height ;y++) {
    for(int x=0; x<window.width ;x++) {
      window.setPixelColour(x, y, get_rgb(WHITE));
    }
  }

  depthbuf.clear();
}

void draw() {
  clearScreen();
  if (current_mode == WIRE) {
    drawGeometry(false);
  }
  else if (current_mode == RASTER) {
    drawGeometry(true);
  }
  else {
    buf_mode = TEXTURE;
    drawGeometry(true);
    buf_mode = WINDOW;
    drawGeometryViaRayTracing();
  }
  //camera.printCamera();
}

void handleEvent(SDL_Event event) {
  bool clear = false;
  if(event.type == SDL_KEYDOWN) {
    if(event.key.keysym.sym == SDLK_p) {
      cout << "P: WRITE PPM FILE" << endl;
      writePPM();
    }
    else if(event.key.keysym.sym == SDLK_i) {
      cout << "I: PRINT INFO" << endl;
      cout << "--------------------------------------------------" << endl;
      camera.printCamera();
      std::cout << "LIGHT position:\n";
      printVec3(light.Position);
      cout << "--------------------------------------------------" << endl;
    }
    else if(event.key.keysym.sym == SDLK_b) {
      cout << "B: DRAW CORNELL BOX (RASTER)" << endl;
      current_mode = RASTER;
    }
    else if(event.key.keysym.sym == SDLK_f) {
      cout << "F: DRAW WIREFRAME" << endl;
      current_mode = WIRE;
    }
    else if(event.key.keysym.sym == SDLK_r) {
      cout << "R: DRAW RAYTRACING" << endl;
      current_mode = RAY;
    }

    else if(event.key.keysym.sym == SDLK_w) {
      cout << "W: MOVE CAMERA FORWARD" << endl;
      camera.moveBy(0, 0, -5);
    }
    else if(event.key.keysym.sym == SDLK_s) {
      cout << "S: MOVE CAMERA BACKWARD" << endl;
      camera.moveBy(0, 0, 5);
    }
    else if(event.key.keysym.sym == SDLK_a) {
      cout << "A: MOVE CAMERA LEFT" << endl;
      camera.moveBy(-5, 0, 0);
    }
    else if(event.key.keysym.sym == SDLK_d) {
      cout << "D: MOVE CAMERA RIGHT" << endl;
      camera.moveBy(5, 0, 0);
    }
    else if(event.key.keysym.sym == SDLK_q) {
      cout << "Q: MOVE CAMERA UP" << endl;
      camera.moveBy(0, 5, 0);
    }
    else if(event.key.keysym.sym == SDLK_e) {
      cout << "E: MOVE CAMERA DOWN" << endl;
      camera.moveBy(0, -5, 0);
    }

    else if(event.key.keysym.sym == SDLK_UP) {
      cout << "UP: TILT CAMERA UP" << endl;
      camera.rotate_X_By(-1.0);
    }
    else if(event.key.keysym.sym == SDLK_DOWN) {
      cout << "DOWN: TILT CAMERA DOWN" << endl;
      camera.rotate_X_By(1.0);
    }
    else if(event.key.keysym.sym == SDLK_LEFT) {
      cout << "LEFT: PAN CAMERA LEFT" << endl;
      camera.rotate_Y_By(-1.0);
    }
    else if(event.key.keysym.sym == SDLK_RIGHT) {
      cout << "RIGHT: PAN CAMERA RIGHT" << endl;
      camera.rotate_Y_By(1.0);
    }
    else if(event.key.keysym.sym == SDLK_z) {
      cout << "Z: ROTATE CAMERA CW" << endl;
      camera.rotate_Z_By(-1.0);
    }
    else if(event.key.keysym.sym == SDLK_x) {
      cout << "X: ROTATE CAMERA ACW" << endl;
      camera.rotate_Z_By(1.0);
    }

    else if(event.key.keysym.sym == SDLK_l) {
      cout << "L: LOOK AT CENTRE OF LOGO" << endl;
      camera.lookAt(getCentreOf("logo"));
    }
    else if(event.key.keysym.sym == SDLK_c) {
      cout << "C: CLEARING SCREEN" << endl;
      clear = true;
    }
    else if(event.key.keysym.sym == SDLK_j) {
      cout << "J: MOVE ALONG ANIM ARC" << endl;
      // Should be -ve, but whatever
      camera.moveAlongAnimArc(10.0f);
      camera.lookAt(getCentreOf("logo"));
    }
    else if(event.key.keysym.sym == SDLK_t) {
      cout << "T: ROTATE TEAPOT" << endl;
      rotateTeaPot(10.0f);
    }
    else if(event.key.keysym.sym == SDLK_g) {
      cout << "G: GO (START ANIMATION)" << endl;
      animating = true;
      frame_no = 0;
    }
    else if(event.key.keysym.sym == SDLK_h) {
      cout << "H: HALT (STOP ANIMATION)" << endl;
      animating = false;
    }
    if (clear)
      clearScreen();
    else
      draw();
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;

  while (animating) {

    if (camera.position.x <= 740.0f || camera.position.x > 4000.0f
        || camera.position.z <= 740.0f || camera.position.z > 4000.0f) {
      //camera.printCamera();
      animating = false;
      std::cout << "\nFINISHED!\n";
    }
    else {
      if(window.pollForInputEvents(&event)) handleEvent(event);

      if (animating) {
        frame_no ++;
        std::cout << "fr_" << frame_no << "; ";
        fflush(stdout);
        draw();
        //writePPM();
        // Wait for the PPM to be written. Increase this value for raster/raytrace
        //SDL_Delay(1000);
        rotateTeaPot(10.0f);
        camera.moveAlongAnimArc(-10.0f);
        camera.lookAt(getCentreOf("logo"));

        window.renderFrame();
      }
    }
  }
}

int main(int argc, char* argv[]) {
  // Initialise globals here, not at top of file, because there, statements
  // are not allowed (so no print statements, or anything, basically)
  readOBJs();

  for(uint i = 0; i < gobjects.size(); i++) {
    if ((gobjects.at(i)).name == "logo") {
      translateGObject(vec3(0.0, 150.0, 700.0), (gobjects.at(i)));
      vec3 avgLogo = getCentreOf("logo");
      camera.position[1] = avgLogo[1];
      camera.position[2] = avgLogo[2];
    }
    // if ((gobjects.at(i)).name == "teapot") {
    //   rotateGObjectAboutYInPlace(225.0f, gobjects.at(i));
    // }

  }

  for (auto g=gobjects.begin(); g != gobjects.end(); g++) {
    cout << "Object " << (*g).name << " is centered at ";
    printVec3(averageVerticesOfFaces((*g).faces));
  }

  texture_buffer = (uint32_t*)malloc(WIDTH*HEIGHT*sizeof(uint32_t));
  window = DrawingWindow(WIDTH, HEIGHT, false);
  depthbuf = DepthBuffer(WIDTH, HEIGHT);
  buf_mode = WINDOW;
  current_mode = WIRE;
  screenshotDir = SCREENSHOT_DIR;
  fs::create_directory(screenshotDir); // ensure it exists

  SDL_Event event;

  draw();

  while(true) {
    if(window.pollForInputEvents(&event)) handleEvent(event);
    window.renderFrame();
  }
}
