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
#define SCREENSHOT_SUFFIX "_screenie.ppm"

fs::path screenshotDir;

Colour COLOURS[] = {Colour(255, 0, 0), Colour(0, 255, 0), Colour(0, 0, 255)};
Colour WHITE = Colour(255, 255, 255);
Colour BLACK = Colour(0, 0, 0);

typedef enum {WIRE, RASTER, RAY} View_mode;

// Global Object Declarations
// ---

OBJ_IO obj_io;
std::vector<GObject> gobjects;
DrawingWindow window;

Texture logoTexture;

DepthBuffer depthbuf;
Camera camera;
View_mode current_mode = WIRE;
Light light;

int number_of_AA_samples = 1;

// Simple Helper Functions
// ---
float max(float A, float B) { if (A > B) return A; return B; }
float min(float A, float B) { if (A < B) return A; return B; }
int modulo(int x, int y) { if (y == 0) return x; return ((x % y) + x) % y; }
bool comparator(CanvasPoint p1, CanvasPoint p2) { return (p1.y < p2.y); }
void printVec3(vec3 v) { cout << "(" << v.x << ", " << v.y << ", " << v.z << ")\n"; }
bool isLight(GObject gobj) { return (gobj.name == "light"); }

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
  fs::path screenshotName = fs::path(getDateTimeString() + SCREENSHOT_SUFFIX);
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

void readOBJs() {
  vector<GObject> cornell;
  vector<GObject> logo;

  // This tuple/optional nonsense is just to avoid changing OBJ_IO/Structure too much.
  // For each obj file we load, it may have a texture file. We should have a global
  // variable for each texture that we know will actually exist and set it to the
  // value of the optional here.
  optional<Texture> maybeCornellTexture; // we know there isn't one, but this shows the usage
  optional<Texture> maybeLogoTexture;

  tie(cornell, maybeCornellTexture) = obj_io.loadOBJ("cornell-box.obj");
  cornell = obj_io.scale(WIDTH, cornell);  // scale each file's objects separately
  tie(logo, maybeLogoTexture) = obj_io.loadOBJ("logo.obj");

  if (maybeLogoTexture)
    logoTexture = maybeLogoTexture.value();
  logo = obj_io.scale(WIDTH, logo);        // scale each file's objects separately
  gobjects = joinGObjectVectors(cornell, logo);

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
    TexturePoint interp_tp(from.texturePoint.x + (i * stepSize_tp_X), from.texturePoint.y + (i * stepSize_tp_Y));
    interp_point.texturePoint = interp_tp;
    v.push_back(interp_point);
  }
  return v;
}

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    if (depthbuf.update(pixel)) window.setPixelColour(round(x), round(y), get_rgb(colour));
  }
}

uint32_t get_textured_pixel(TexturePoint texturePoint, Texture texture) {
 return texture.ppm_image[(int)(round(texturePoint.x) + (round(texturePoint.y) * texture.width))];
}

void drawTexturedLine(CanvasPoint P1, CanvasPoint P2) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    uint32_t colour = get_textured_pixel(pixel.texturePoint, logoTexture);
    if (round(x) >= 0 && round(x) < WIDTH && round(y) >= 0 && round(y) < HEIGHT) {
      if (depthbuf.update(pixel)) window.setPixelColour(round(x), round(y), colour);
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
  if (triangle.textured) drawTexturedLine(side1.at(0), side2.at(0));
  else drawLine(side1.at(0), side2.at(0), triangle.colour);

  for (uint i = 0; i < side1.size(); i++) {
    if (round(side1.at(i).y) != last_drawn_y) {
      int j = 0;
      while ((j < (int)side2.size() - 1) && (round(side2.at(j).y) <= last_drawn_y)) {
	       j++;
      }
      if (triangle.textured) drawTexturedLine(side1.at(i), side2.at(j));
      else drawLine(side1.at(i), side2.at(j), triangle.colour);
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
  for (int i = 0; i < 3; i++) {
    result.vertices[i] = projectVertexInto2D(triangle.vertices[i]);
    if (triangle.maybeTextureTriangle) {
      result.vertices[i].texturePoint.x = triangle.maybeTextureTriangle.value().vertices[i][0];
      result.vertices[i].texturePoint.y = triangle.maybeTextureTriangle.value().vertices[i][1];
      result.vertices[i].texturePoint.x *= logoTexture.width;
      result.vertices[i].texturePoint.y *= logoTexture.height;
    }
    else {
      result.vertices[i].texturePoint = TexturePoint(-1, -1);
    }
  }
  result.colour = triangle.colour;
  if (triangle.maybeTextureTriangle) {
    result.textured = true;
  }
  else {
    result.textured = false;
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

RayTriangleIntersection getFarthestIntersection(ModelTriangle triangle, int x, int y, float slider) {
  mat3 adjOrientation(camera.orientation[0], -camera.orientation[1], camera.orientation[2]);
  bool stillSolution = true;
  float x_off = 0;
  RayTriangleIntersection sol;

  glm::vec3 rayDir = vec3(x, y, camera.focalLength) * adjOrientation;
  RayTriangleIntersection possibleSolution = getPossibleIntersection(triangle, rayDir, camera.position);

  while (stillSolution) {
    sol = possibleSolution;
    x_off += slider;
    rayDir = vec3(x + x_off, y, camera.focalLength) * adjOrientation;
    possibleSolution = getPossibleIntersection(triangle, rayDir, camera.position);
    if (!possibleSolution.isSolution) {
      stillSolution = false;
    }
  }

  return sol;
}

float getTextureX(RayTriangleIntersection intersection, int i, int j) {
  // Iteratively cast out rays left and right to find the proportion of the point along the rake.
  // Then use the edge points to calculate the proportion along that row.
  // Interpolate using these values to get the texture point X value.
  ModelTriangle triangle = intersection.intersectedTriangle;
  TextureTriangle textureTriangle = triangle.maybeTextureTriangle.value();
  mat3 adjOrientation(camera.orientation[0], -camera.orientation[1], camera.orientation[2]);

  int x =  i - WIDTH / 2;
  int y = -j + HEIGHT / 2;
  float slider = 0.01f;

  RayTriangleIntersection sol_L = getFarthestIntersection(triangle, x, y, -slider);
  RayTriangleIntersection sol_R = getFarthestIntersection(triangle, x, y,  slider);

  vec3 P1 = (intersection.intersectionPoint - sol_L.intersectionPoint);
  vec3 P2 = (sol_R.intersectionPoint - sol_L.intersectionPoint);

  float prop_along_rake = 0.0f;
  if (glm::length(P2) != 0) prop_along_rake = glm::length(P1) / glm::length(P2);

  float prop_along_side1 = sol_L.u;
  float prop_along_side2 = sol_R.v;

  vec2 tex_1 = vec2((textureTriangle.vertices[0] * prop_along_side1) + (textureTriangle.vertices[1] * (1 - prop_along_side1)));
  vec2 tex_2 = vec2((textureTriangle.vertices[0] * prop_along_side2) + (textureTriangle.vertices[2] * (1 - prop_along_side2)));

  vec2 tex_rake = vec2((tex_1 * prop_along_rake) + (tex_2 * (1 - prop_along_rake)));
  //std::cout << "tex_rake.x: " << tex_rake.x << '\n';

  return tex_rake.x;
}

Colour getTextureColourFromIntersection(RayTriangleIntersection intersection, int i, int j) {
  //std::cout << "------------------------------------------------" << "\n";
  //std::cout << "i: " << i << ", j: " << j << '\n';

  //C = ((C_0 / Z_0)(1 - q) + (C_1 / Z_1)(q)) / ((1 / Z_0)(1 - q) + (1 / Z_1)(q))
  // this formula defines perspective-corrected texture mapping
  // where C is the row of the texture image we should use.
  ModelTriangle iTriangle =  intersection.intersectedTriangle;
  TextureTriangle textureTriangle = iTriangle.maybeTextureTriangle.value();

  int vertex_fars = 0, vertex_clos = 0;
  float dist_clos, dist_fars, distToCam;

  for (int v = 0; v < 3; v++) {
    distToCam = glm::length(iTriangle.vertices[v] - camera.position);
    dist_fars = glm::length(iTriangle.vertices[vertex_fars] - camera.position);
    dist_clos = glm::length(iTriangle.vertices[vertex_clos] - camera.position);

    //std::cout << v << " to cam: " << distToCam << ", ";
    if (distToCam > dist_fars) vertex_fars = v;
    if (distToCam < dist_clos) vertex_clos = v;
  }

  CanvasPoint Z_0_CP = projectVertexInto2D(iTriangle.vertices[vertex_fars]);
  CanvasPoint Z_1_CP = projectVertexInto2D(iTriangle.vertices[vertex_clos]);
  CanvasPoint   q_CP = projectVertexInto2D(intersection.intersectionPoint);
  // q is the distance between the y coordinate of the pixel and the y coordinate of Z_0
  // ON THE CANVAS!

  float Z_0 = glm::length(iTriangle.vertices[vertex_fars] - camera.position);;
  float Z_1 = glm::length(iTriangle.vertices[vertex_clos] - camera.position);;
  float C_0 = textureTriangle.vertices[vertex_fars].y * logoTexture.height;
  float C_1 = textureTriangle.vertices[vertex_clos].y * logoTexture.height;
  float   q = (round(Z_0_CP.y) - round(q_CP.y)) / (round(Z_0_CP.y) - round(Z_1_CP.y));

  float C = (((C_0 / Z_0) * (1 - q)) + ((C_1 / Z_1) * (q))) / (((1 / Z_0) * (1 - q)) + ((1 / Z_1) * (q)));

  //std::cout << "Projected Z_0 CanvasPoint: " << Z_0_CP;
  //std::cout << "Projected Z_1 CanvasPoint: " << Z_1_CP;
  //std::cout << "Projected   q CanvasPoint: " << q_CP << '\n';
  //std::cout << "Closest Point:  " << vertex_clos << ", "; printVec3(iTriangle.vertices[vertex_clos]);
  //std::cout << "Farthest Point: " << vertex_fars << ", "; printVec3(iTriangle.vertices[vertex_fars]);
  //std::cout << "\nZ_0: " << Z_0 << ", Z_1: " << Z_1 << ", C_0: " << C_0 << ", C_1: " << C_1 << ", q: " << q << '\n';
  //std::cout << "C: " << C << "\n\n";

  int texture_Y = round(C);
  int texture_X = round(getTextureX(intersection, i, j) * logoTexture.width);

  //std::cout << "proportion_along_rake: " << proportion_along_rake << '\n';
  //std::cout << "Y: " << texture_Y << ", X: " << texture_X << '\n';

  uint32_t texpix = logoTexture.ppm_image[texture_X + (texture_Y * logoTexture.width)];
  uint8_t red   = (texpix >> 16) & 0xFF;
  uint8_t green = (texpix >> 8) & 0xFF;
  uint8_t blue  = (texpix) & 0xFF;
  return Colour(red, green, blue);
}

Colour getAdjustedColour(RayTriangleIntersection intersection, int i, int j) {
  Colour inputColour = intersection.intersectedTriangle.colour;
  if (intersection.intersectedTriangle.maybeTextureTriangle)
    inputColour = getTextureColourFromIntersection(intersection, i, j);

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
      cout << "L: LOOK AT (0,5,-5)" << endl;
      camera.lookAt(vec3(0,5,-5));
    }
    else if(event.key.keysym.sym == SDLK_c) {
      cout << "C: CLEARING SCREEN" << endl;
      clear = true;
    }
    if (clear)
      clearScreen();
    else
      draw();
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;
}

int main(int argc, char* argv[]) {
  // Initialise globals here, not at top of file, because there, statements
  // are not allowed (so no print statements, or anything, basically)
  readOBJs();

  window = DrawingWindow(WIDTH, HEIGHT, false);
  depthbuf = DepthBuffer(WIDTH, HEIGHT);

  screenshotDir = SCREENSHOT_DIR;
  fs::create_directory(screenshotDir); // ensure it exists

  SDL_Event event;

  draw();

  while(true) {
    if(window.pollForInputEvents(&event)) handleEvent(event);
    window.renderFrame();
  }
}
