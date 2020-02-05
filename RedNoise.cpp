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
void drawLine(float from_X, float from_Y, float to_X, float to_Y, uint32_t colour);
std::vector<double> interpolate(double from, double to, int numberOfValues);
std::vector<vec3> interpolate_vec3(glm::vec3 from, glm::vec3 to, int numberOfValues);

DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);

int main(int argc, char* argv[])
{
  interpolate(2.2, 8.5, 7);
  vec3 from( 1, 4, 9.2 );
  vec3 to( 4, 1, 9.8 );
  interpolate_vec3(from, to, 4);
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

float max(float A, float B) { if (A > B) return A; return B; }

void drawLine(float from_X, float from_Y, float to_X, float to_Y, uint32_t colour) {
  float delta_X  = to_X - from_X;
  float delta_Y  = to_Y - from_Y;
  float no_steps = max(abs(delta_X), abs(delta_Y));
  //std::cout << "Steps: " << no_steps << " delta_X: " << delta_X << " delta_Y: " << delta_Y << '\n';
  float stepSize_X = delta_X / no_steps;
  float stepSize_Y = delta_Y / no_steps;
  //std::cout << "StepSizes: X: " << stepSize_X << " Y: " << stepSize_Y << '\n';

  for (float i = 0.0; i < no_steps; i++) {
    float x = from_X + (i * stepSize_X);
    float y = from_Y + (i * stepSize_Y);
    window.setPixelColour(x, y, colour);
  }
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
  uint32_t lineColour = 0;
  drawLine(50, 50, 100, 100, lineColour);
  drawLine(50, 50, 50, 100, lineColour);
  drawLine(50, 50, 100, 50, lineColour);
  drawLine(100, 50, 50, 100, lineColour);
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
