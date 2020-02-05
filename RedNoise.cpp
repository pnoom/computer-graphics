#include <ModelTriangle.h>
#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <glm/glm.hpp>
#include <fstream>
#include <vector>

using namespace std;
using namespace glm;

#define WIDTH 320
#define HEIGHT 240

void draw();
void update();
void handleEvent(SDL_Event event);

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour);
void drawStrokedTriangle(CanvasTriangle triangle);
void sortTrianglePoints(CanvasTriangle *triangle);
void drawFilledTriangle(CanvasTriangle triangle);

std::vector<double> interpolate(double from, double to, int numberOfValues);
std::vector<vec3> interpolate_vec3(glm::vec3 from, glm::vec3 to, int numberOfValues);
std::vector<CanvasPoint> interpolate_line(CanvasPoint from, CanvasPoint to);

DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
CanvasTriangle triangle(CanvasPoint(50, 100), CanvasPoint(0, 80), CanvasPoint(90, 200), Colour(0, 0, 0));

float max(float A, float B) { if (A > B) return A; return B; }
uint32_t ColourPacker(Colour colour) { return (0 << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue; }

int main(int argc, char* argv[])
{
  sortTrianglePoints(&triangle);
  SDL_Event event;
  while(true)
  {
    // We MUST poll for events - otherwise the window will freeze !
    if(window.pollForInputEvents(&event)) handleEvent(event);
    update();
    draw();
    // Need to render the frame at the end, or nothing actually gets shown on the screen !
    window.renderFrame();
  }
}

std::vector<double> interpolate(double from, double to, int numberOfValues) {
  double delta = (to - from) / (numberOfValues - 1);
  std::vector<double> v;
  for (int i = 0; i < numberOfValues; i++) v.push_back(from + (i * delta));
  //for (int i = 0; i < numberOfValues; i++) std::cout << v.at(i) << '\n';
  return v;
}

std::vector<vec3> interpolate_vec3(glm::vec3 from, glm::vec3 to, int numberOfValues) {
  double delta_1, delta_2, delta_3;
  delta_1 = (to[0] - from[0]) / (numberOfValues - 1);
  delta_2 = (to[1] - from[1]) / (numberOfValues - 1);
  delta_3 = (to[2] - from[2]) / (numberOfValues - 1);
  //std::cout << delta_1 << " " << delta_2 << " " << delta_3 << '\n';

  std::vector<vec3> v;
  for (int i = 0; i < numberOfValues; i++) {
    double vec3_x = from[0] + (i * delta_1);
    double vec3_y = from[1] + (i * delta_2);
    double vec3_z = from[2] + (i * delta_3);
    vec3 interp_vec3(vec3_x, vec3_y, vec3_z);
    v.push_back(interp_vec3);
  }
  //for (int i = 0; i < numberOfValues; i++) std::cout << v.at(i)[0] << " " << v.at(i)[1] << " " << v.at(i)[2] << '\n';
  return v;
}

std::vector<CanvasPoint> interpolate_line(CanvasPoint from, CanvasPoint to) {
  float delta_X  = to.x - from.x;
  float delta_Y  = to.y - from.y;
  float no_steps = max(abs(delta_X), abs(delta_Y));
  //std::cout << "Steps: " << no_steps << " delta_X: " << delta_X << " delta_Y: " << delta_Y << '\n';
  float stepSize_X = delta_X / no_steps;
  float stepSize_Y = delta_Y / no_steps;

  std::vector<CanvasPoint> v;
  for (float i = 0.0; i < no_steps; i++) {
    CanvasPoint interp_point(from.x + (i * stepSize_X), from.y + (i * stepSize_Y));
    v.push_back(interp_point);
  }
  return v;
}

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (float i = 0.0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at((int)i);
    float x = pixel.x;
    float y = pixel.y;
    window.setPixelColour(x, y, ColourPacker(colour));
  }
}

void drawStrokedTriangle(CanvasTriangle triangle) {
  drawLine(triangle.vertices[0], triangle.vertices[1], triangle.colour);
  drawLine(triangle.vertices[0], triangle.vertices[2], triangle.colour);
  drawLine(triangle.vertices[1], triangle.vertices[2], triangle.colour);
}

void sortTrianglePoints(CanvasTriangle *triangle) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (triangle->vertices[i].y < triangle->vertices[j].y) swap(triangle->vertices[i], triangle->vertices[j]);
    }
  }
  //std::cout << triangle.vertices[0].y << " " << triangle.vertices[1].y << " " << triangle.vertices[2].y << '\n';
}

void equateTriangles(CanvasTriangle from, CanvasTriangle *to) {
  for (int i = 0; i < 3; i++) {
    to->vertices[i].x = from.vertices[i].x;
    to->vertices[i].y = from.vertices[i].y;
  }
  to->colour = from.colour;
}

void getTopBottomTriangles(CanvasTriangle triangle, CanvasTriangle *top, CanvasTriangle *bottom) {
  float x4;

  float delta_X  = triangle.vertices[0].x - triangle.vertices[2].x;
  float delta_Y  = triangle.vertices[0].y - triangle.vertices[2].y;
  float no_steps = max(abs(delta_X), abs(delta_Y));

  float stepSize_X = delta_X / no_steps;
  float stepSize_Y = delta_Y / no_steps;

  for (float i = 0.0; i < no_steps; i++) {
    if (round(triangle.vertices[1].y) == round(triangle.vertices[2].y + (i * stepSize_Y)))
      x4 = triangle.vertices[2].x + (i * stepSize_X);
  }
  CanvasPoint point4(x4, triangle.vertices[1].y);
  //CONSTRUCTOR: CanvasTriangle(CanvasPoint v0, CanvasPoint v1, CanvasPoint v2, Colour c)
  CanvasTriangle top_Triangle(triangle.vertices[0], point4, triangle.vertices[1], triangle.colour);
  CanvasTriangle bot_Triangle(point4, triangle.vertices[1], triangle.vertices[2], triangle.colour);
  //drawStrokedTriangle(top_Triangle);
  //drawStrokedTriangle(bot_Triangle);
  equateTriangles(top_Triangle, top);
  equateTriangles(bot_Triangle, bottom);
}

void drawFilledTriangle(CanvasTriangle triangle) {
  CanvasTriangle triangles[2];
  getTopBottomTriangles(triangle, &triangles[0], &triangles[1]);
  drawStrokedTriangle(triangles[0]);
  drawStrokedTriangle(triangles[1]);
}

uint32_t get_rgb(vec3 rgb) {
  uint32_t result = (255<<24) + (int(rgb[0])<<16) + (int(rgb[1])<<8) + int(rgb[2]);
  return result;
}

void draw()
{
  window.clearPixels();

  for(int y=0; y<window.height ;y++) {
    for(int x=0; x<window.width ;x++) {
      uint32_t colour = (0 << 24) + (255 << 16) + (255 << 8) + 255;
      window.setPixelColour(x, y, colour);
    }
  }

  drawFilledTriangle(triangle);
}

void update()
{
  // Function for performing animation (shifting artifacts or moving the camera)
}

void handleEvent(SDL_Event event)
{
  if(event.type == SDL_KEYDOWN) {
    if(event.key.keysym.sym == SDLK_LEFT) cout << "LEFT" << endl;
    else if(event.key.keysym.sym == SDLK_RIGHT) cout << "RIGHT" << endl;
    else if(event.key.keysym.sym == SDLK_UP) cout << "UP" << endl;
    else if(event.key.keysym.sym == SDLK_DOWN) cout << "DOWN" << endl;
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;
}
