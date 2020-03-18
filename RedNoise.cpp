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

#include "Texture.hpp"
#include "GObject.hpp"
#include "OBJ_IO.hpp"
#include "Camera.hpp"
#include "DepthBuffer.hpp"

using namespace std;
using namespace glm;

// Definitions
// ---
#define WIDTH 500
#define HEIGHT 500

#define OUTPUT_FILE "screenie.ppm"

Colour COLOURS[] = {Colour(255, 0, 0), Colour(0, 255, 0), Colour(0, 0, 255)};
Colour WHITE = Colour(255, 255, 255);
Colour BLACK = Colour(0, 0, 0);

typedef enum {WIRE, RASTER, RAY} View_mode;

// Global Object Instantiations
// ---
DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);

Texture the_image("cobbles.ppm");
DepthBuffer depthbuf(WIDTH, HEIGHT);
Camera camera;
View_mode current_mode = WIRE;
OBJ_IO obj_io;
std::vector<GObject> gobjects = obj_io.loadOBJ("cornell-box.obj", WIDTH);
vec3 lightPos(250.0f, 470.0f, 120.0f);
//vec3 lightPos(250.0f, 400.0f, 200.0f);
float lightIntensity = 2000.0f;
float lightSpread = 4.0f;

// Simple Helper Functions
// ---
float max(float A, float B) { if (A > B) return A; return B; }
float min(float A, float B) { if (A < B) return A; return B; }
int modulo(int x, int y) { return ((x % y) + x) % y; }
bool comparator(CanvasPoint p1, CanvasPoint p2) { return (p1.y < p2.y); }
void printVec3(vec3 v) { cout << "(" << v.x << ", " << v.y << ", " << v.z << ")\n"; }

uint32_t get_rgb(Colour colour) { return (0 << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue; }
uint32_t get_rgb(vec3 rgb) { return (uint32_t)((255<<24) + (int(rgb[0])<<16) + (int(rgb[1])<<8) + int(rgb[2])); }

void writePPM(string filename) {
  FILE* f = fopen(filename.c_str(), "w");
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

uint32_t get_textured_pixel(TexturePoint texturePoint) {
  return the_image.ppm_image[(int)(round(texturePoint.x) + (round(texturePoint.y) * the_image.width))];
}

void drawTexturedLine(CanvasPoint P1, CanvasPoint P2) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    uint32_t colour = get_textured_pixel(pixel.texturePoint);
    //uint32_t colour = 0;
    window.setPixelColour(round(x), round(y), colour);
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
  CanvasTriangle top_Triangle(triangle.vertices[0], point4, triangle.vertices[1], triangle.colour);
  CanvasTriangle bot_Triangle(point4, triangle.vertices[2], triangle.vertices[1], triangle.colour);

  equateTriangles(top_Triangle, top);
  equateTriangles(bot_Triangle, bottom);
}

void fillFlatBaseTriangle(CanvasTriangle triangle, int side1_A, int side1_B, int side2_A, int side2_B, bool textured) {
  std::vector<CanvasPoint> side1 = interpolate_line(triangle.vertices[side1_A], triangle.vertices[side1_B]);
  std::vector<CanvasPoint> side2 = interpolate_line(triangle.vertices[side2_A], triangle.vertices[side2_B]);

  uint last_drawn_y = round(side1.at(0).y);

  if (textured) drawTexturedLine(side1.at(0), side2.at(0));
  else drawLine(side1.at(0), side2.at(0), triangle.colour);

  for (uint i = 0; i < side1.size(); i++) {
    if (round(side1.at(i).y) != last_drawn_y) {
      int j = 0;
      while ((j < (int)side2.size() - 1) && (round(side2.at(j).y) <= last_drawn_y)) {
	       j++;
      }
      if (textured) drawTexturedLine(side1.at(i), side2.at(j));
      else drawLine(side1.at(i), side2.at(j), triangle.colour);
      last_drawn_y++;
    }
  }
}

bool triangleIsLine(CanvasTriangle triangle) {
  std::vector<CanvasPoint> v = interpolate_line(triangle.vertices[0], triangle.vertices[1]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[2].x)) && (round(v.at(i).y) == round(triangle.vertices[2].y))) {
      return true;
    }
  }

  v = interpolate_line(triangle.vertices[0], triangle.vertices[2]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[1].x)) && (round(v.at(i).y) == round(triangle.vertices[1].y))) {
      return true;
    }
  }

  v = interpolate_line(triangle.vertices[1], triangle.vertices[2]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[0].x)) && (round(v.at(i).y) == round(triangle.vertices[0].y))) {
      return true;
    }
  }

  return false;
}

void drawFilledTriangle(CanvasTriangle triangle, bool textured) {
  sortTrianglePoints(&triangle);
  CanvasTriangle triangles[2];

  if (triangleIsLine(triangle)) {
    drawLine(triangle.vertices[0], triangle.vertices[1], triangle.colour);
    drawLine(triangle.vertices[1], triangle.vertices[2], triangle.colour);
  }
  else {
    getTopBottomTriangles(triangle, &triangles[0], &triangles[1]);
    fillFlatBaseTriangle(triangles[0], 0, 1, 0, 2, textured);
    fillFlatBaseTriangle(triangles[1], 0, 1, 2, 1, textured);
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
  }
  result.colour = triangle.colour;

  return result;
}

// Raytracing Functions
// ---
RayTriangleIntersection getPossibleIntersection(ModelTriangle triangle, glm::vec3 rayDir, glm::vec3 point, bool canCout) {
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
        RayTriangleIntersection res(point3d, t * glm::length(rayDir), triangle, true);
        return res;
  }

  return RayTriangleIntersection();
}

RayTriangleIntersection getClosestIntersection(glm::vec3 rayDir) {
  RayTriangleIntersection closestIntersectionFound = RayTriangleIntersection();

  for (uint j=0; j<gobjects.size(); j++) {
    for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
      ModelTriangle triangle = gobjects.at(j).faces.at(i);
      RayTriangleIntersection possibleSolution = getPossibleIntersection(triangle, rayDir, camera.position, false);

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

  glm::vec3 point_to_light = -lightPos + point;
  point_to_light = glm::normalize(point_to_light);

  float AOI = glm::dot(norm_1, point_to_light);
  if ((AOI < 0.0f) || (AOI >= 1.0f)) {
    AOI = glm::dot(norm_2, point_to_light);
  }
  return AOI;
}

float getIntensity(glm::vec3 point) {
  glm::vec3 point_to_light = -lightPos + point;
  float intensity =  lightIntensity / (lightSpread * M_PI * glm::length(point_to_light));
  return intensity;
}

bool isPointInShadow(glm::vec3 point, ModelTriangle self) {
  glm::vec3 rayDir = -lightPos + point;
  for (uint j=0; j<gobjects.size(); j++) {
    for (uint i=0; i<gobjects.at(j).faces.size(); i++) {
      ModelTriangle triangle = gobjects.at(j).faces.at(i);
      if (!compareTriangles(self, triangle)) {
        RayTriangleIntersection intersection = getPossibleIntersection(triangle, rayDir, lightPos, true);
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

Colour getAdjustedColour(RayTriangleIntersection intersection) {
  Colour inputColour = intersection.intersectedTriangle.colour;
  float AOI = getAngleOfIncidence(intersection.intersectionPoint, intersection.intersectedTriangle);
  float intensity = getIntensity(intersection.intersectionPoint);
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
  for (int j=0; j<HEIGHT; j++) {
    for (int i=0; i<WIDTH; i++) {
      // Note: the sign of the y value here is flipped
      //    in pixelRay and adjOrientation
      //    to ensure continuity between raytracer and rasteriser
      glm::vec3 pixelRay(i - WIDTH/2, -j + HEIGHT/2, camera.focalLength);
      mat3 adjOrientation(camera.orientation[0], -camera.orientation[1], camera.orientation[2]);
      pixelRay = pixelRay * adjOrientation;

      RayTriangleIntersection intersection = getClosestIntersection(pixelRay);

      if (intersection.isSolution) {
        Colour adjustedColour = getAdjustedColour(intersection);
        uint32_t colour = get_rgb(adjustedColour);

        window.setPixelColour(i, j, colour);
      }
      else {
        window.setPixelColour(i, j, get_rgb(BLACK));
      }
    }
  }
}

void drawGeometry(bool filled) {
  for (uint i = 0; i < gobjects.size(); i++) {
    for (uint j = 0; j < gobjects.at(i).faces.size(); j++) {
      CanvasTriangle projectedTriangle = projectTriangleOntoImagePlane(gobjects.at(i).faces.at(j));
      if (filled) drawFilledTriangle(projectedTriangle, false);
      else drawStrokedTriangle(projectedTriangle);
    }
  }

  CanvasPoint lightCP = projectVertexInto2D(lightPos);
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
  camera.printCamera();
}

void handleEvent(SDL_Event event) {
  if(event.type == SDL_KEYDOWN) {
    if(event.key.keysym.sym == SDLK_p) {
      cout << "P: WRITE PPM FILE" << endl;
      writePPM(OUTPUT_FILE);
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
      cout << "UP: ROTATE CAMERA UP" << endl;
      camera.rotate_X_By(-1.0);
    }
    else if(event.key.keysym.sym == SDLK_DOWN) {
      cout << "DOWN: ROTATE CAMERA DOWN" << endl;
      camera.rotate_X_By(1.0);
    }
    else if(event.key.keysym.sym == SDLK_LEFT) {
      cout << "LEFT: ROTATE CAMERA ANTICLOCKWISE Y AXIS" << endl;
      camera.rotate_Y_By(-1.0);
    }
    else if(event.key.keysym.sym == SDLK_RIGHT) {
      cout << "RIGHT: ROTATE CAMERA CLOCKWISE ABOUT Y AXIS" << endl;
      camera.rotate_Y_By(1.0);
    }

    else if(event.key.keysym.sym == SDLK_l) {
      cout << "L: LOOK AT (0,5,-5)" << endl;
      camera.lookAt(vec3(0,5,-5));
    }
    draw();
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;

  if(event.key.keysym.sym == SDLK_c) {
    cout << "C: CLEARING SCREEN" << endl;
    clearScreen();
  }
}

int main(int argc, char* argv[]) {
  SDL_Event event;

  draw();
  camera.printCamera();
  std::cout << "LIGHT position:\n";
  printVec3(lightPos);

  while(true) {
    if(window.pollForInputEvents(&event)) handleEvent(event);
    window.renderFrame();
  }
}
