#include <ModelTriangle.h>
#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <glm/glm.hpp>
#include <fstream>
#include <vector>

using namespace std;
using namespace glm;

#define WIDTH 500
#define HEIGHT 500

void draw();
void update();
void handleEvent(SDL_Event event);

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour);
void drawStrokedTriangle(CanvasTriangle triangle);
void sortTrianglePoints(CanvasTriangle *triangle);
void drawFilledTriangle(CanvasTriangle triangle);
void loadPPM(char *imageName);

std::vector<double> interpolate(double from, double to, int numberOfValues);
std::vector<vec3> interpolate_vec3(glm::vec3 from, glm::vec3 to, int numberOfValues);
std::vector<CanvasPoint> interpolate_line(CanvasPoint from, CanvasPoint to);

DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
CanvasTriangle triangle(CanvasPoint(50, 100), CanvasPoint(0, 80), CanvasPoint(90, 200), Colour(255, 0, 0));

float max(float A, float B) { if (A > B) return A; return B; }
uint32_t get_rgb(Colour colour) { return (0 << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue; }
uint32_t get_rgb(vec3 rgb) { return (uint32_t)((255<<24) + (int(rgb[0])<<16) + (int(rgb[1])<<8) + int(rgb[2])); }

int main(int argc, char* argv[])
{
  sortTrianglePoints(&triangle);
  SDL_Event event;
  while(true)
  {
    // We MUST poll for events - otherwise the window will freeze !
    if(window.pollForInputEvents(&event)) handleEvent(event);
    loadPPM("texture.ppm");
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

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    window.setPixelColour(round(x), round(y), get_rgb(colour));
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
  CanvasTriangle bot_Triangle(point4, triangle.vertices[2], triangle.vertices[1], triangle.colour);
  //drawStrokedTriangle(top_Triangle);
  //drawStrokedTriangle(bot_Triangle);
  equateTriangles(top_Triangle, top);
  equateTriangles(bot_Triangle, bottom);
}

void fillFlatBaseTriangle(CanvasTriangle triangle, int side1_A, int side1_B, int side2_A, int side2_B) {

  std::vector<CanvasPoint> side1 = interpolate_line(triangle.vertices[side1_A], triangle.vertices[side1_B]);
  std::vector<CanvasPoint> side2 = interpolate_line(triangle.vertices[side2_A], triangle.vertices[side2_B]);

  //std::cout << "side1.size() " << side1.size() << " side2.size() " << side2.size() << "\n";
  uint last_drawn_y = round(side1.at(0).y);
  drawLine(side1.at(0), side2.at(0), triangle.colour);
  for (uint i = 0; i < side1.size(); i++) {
    if (round(side1.at(i).y) != last_drawn_y) {
      int j = 0;
      while (round(side2.at(j).y) <= last_drawn_y) {
	       j++;
      }
      //std::cout << "i " << round(side1.at(i).y) << " j " << round(side2.at(j).y) << "\n";
      drawLine(side1.at(i), side2.at(j), triangle.colour);
      last_drawn_y++;
    }
  }
}

void drawFilledTriangle(CanvasTriangle triangle) {
  CanvasTriangle triangles[2];
  getTopBottomTriangles(triangle, &triangles[0], &triangles[1]);

  drawStrokedTriangle(triangles[0]);
  drawStrokedTriangle(triangles[1]);
  fillFlatBaseTriangle(triangles[0], 0, 1, 0, 2);
  fillFlatBaseTriangle(triangles[1], 0, 1, 2, 1);
}

void parse_size(char *size, int height, int width) {
  height = 395;
  width = 480;
}

int parse_maxcolour(char *maxcolour) {
  return 255;
}

void loadPPM(char *imageName) {
  /*
  FILE* f = fopen(imageName, "r");
  char buf_p_six[10], buf_size[20], buf_maxcolour[10], buf[3];
  int height, width, maxcolour;
  bool loaded_PPM = true;

  if (f == NULL) loaded_PPM = false;
  else {
    fgets(buf_p_six, 100, f);
    if(buf_p_six[0] != 'P' && buf_p_six[1] != '6') loaded_PPM = false;
    fgets(buf_size, 100, f);
    parse_size(buf_size, height, width);
    fgets(buf_maxcolour, 100, f);
    maxcolour = parse_maxcolour(buf_maxcolour);

    int x = 0, y = 0;
    while(!feof(f) && loaded_PPM) {
      if (fgets(buf, 3, f) == NULL) loaded_PPM = false;
      uint32_t colour = (buf[0] << 16) + (buf[1] << 8) + buf[2];
      window.setPixelColour(x, y, colour);
      x++;
      y++;
      if (x == width) x = 0;
    }
    fclose(f);
  }
  */
}

void draw() {
  window.clearPixels();

  for(int y=0; y<window.height ;y++) {
    for(int x=0; x<window.width ;x++) {
      uint32_t colour = (0 << 24) + (255 << 16) + (255 << 8) + 255;
      window.setPixelColour(x, y, colour);
    }
  }

  drawFilledTriangle(triangle);
}

void update() {
  // Function for performing animation (shifting artifacts or moving the camera)
}

void handleEvent(SDL_Event event) {
  if(event.type == SDL_KEYDOWN) {
    if(event.key.keysym.sym == SDLK_LEFT) cout << "LEFT" << endl;
    else if(event.key.keysym.sym == SDLK_RIGHT) cout << "RIGHT" << endl;
    else if(event.key.keysym.sym == SDLK_UP) cout << "UP" << endl;
    else if(event.key.keysym.sym == SDLK_DOWN) cout << "DOWN" << endl;
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;
}
