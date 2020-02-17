#include <ModelTriangle.h>
#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <unordered_map>
#include <string>

using namespace std;
using namespace glm;

#define WIDTH 500
#define HEIGHT 500

Colour COLOURS[] = {Colour(255, 0, 0), Colour(0, 255, 0), Colour(0, 0, 255)};

void draw();
void update();
void handleEvent(SDL_Event event);

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour);
void drawStrokedTriangle(CanvasTriangle triangle);
void sortTrianglePoints(CanvasTriangle *triangle);
void drawFilledTriangle(CanvasTriangle triangle, bool textured);
uint32_t* loadPPM(string imageName, int* the_width, int* the_height);
uint32_t* the_image;
int the_image_width, the_image_height;

std::vector<double> interpolate(double from, double to, int numberOfValues);
std::vector<vec3> interpolate_vec3(glm::vec3 from, glm::vec3 to, int numberOfValues);
std::vector<CanvasPoint> interpolate_line(CanvasPoint from, CanvasPoint to);

DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);

float max(float A, float B) { if (A > B) return A; return B; }
uint32_t get_rgb(Colour colour) { return (0 << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue; }
uint32_t get_rgb(vec3 rgb) { return (uint32_t)((255<<24) + (int(rgb[0])<<16) + (int(rgb[1])<<8) + int(rgb[2])); }

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
  float delta_tp_X = to.texturePoint.x - from.texturePoint.x;
  float delta_tp_Y = to.texturePoint.y - from.texturePoint.y;

  float no_steps = max(abs(delta_X), abs(delta_Y));
  //std::cout << "Steps: " << no_steps << " delta_X: " << delta_X << " delta_Y: " << delta_Y << '\n';
  float stepSize_X = delta_X / no_steps;
  float stepSize_Y = delta_Y / no_steps;
  float stepSize_tp_X = delta_tp_X / no_steps;
  float stepSize_tp_Y = delta_tp_Y / no_steps;

  std::vector<CanvasPoint> v;
  for (float i = 0.0; i < no_steps; i++) {
    CanvasPoint interp_point(from.x + (i * stepSize_X), from.y + (i * stepSize_Y));
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
    window.setPixelColour(round(x), round(y), get_rgb(colour));
  }
}

uint32_t get_textured_pixel(TexturePoint texturePoint) {
  return the_image[(int)(round(texturePoint.x) + (round(texturePoint.y) * the_image_width))];
}

void drawTexturedLine(CanvasPoint P1, CanvasPoint P2) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    uint32_t colour = get_textured_pixel(pixel.texturePoint);
    window.setPixelColour(round(x), round(y), colour);
  }
}

int modulo(int x, int y) {
  return ((x % y) + x) % y;
}

void generateTriangle(CanvasTriangle *triangle) {
  float vertices[6];
  //std::cout << "VERTICES: ";
  for (int i = 0; i < 6; i++) {
    vertices[i] = (float)(modulo(rand(), WIDTH));
  }
  //for (int i = 0; i < 3; i++) std::cout << i << ": (" << vertices[i * 2] << ", " << vertices[i * 2 + 1] << ") ";
  //std::cout << '\n';
  triangle->vertices[0] = CanvasPoint(vertices[0], vertices[1]);
  triangle->vertices[1] = CanvasPoint(vertices[2], vertices[3]);
  triangle->vertices[2] = CanvasPoint(vertices[4], vertices[5]);
  triangle->colour = COLOURS[modulo(rand(), 3)];
  //triangle->colour = Colour(255, 255, 255);
}

void drawStrokedTriangle(CanvasTriangle triangle) {
  drawLine(triangle.vertices[0], triangle.vertices[1], Colour(0,0,0));
  drawLine(triangle.vertices[0], triangle.vertices[2], Colour(0,0,0));
  drawLine(triangle.vertices[1], triangle.vertices[2], Colour(0,0,0));
}

bool comparator(CanvasPoint p1, CanvasPoint p2) {
  return (p1.y < p2.y);
}

void sortTrianglePoints(CanvasTriangle *triangle) {
  std::sort(std::begin(triangle->vertices), std::end(triangle->vertices), comparator);
  //std::cout << "SORTED: " << triangle->vertices[0].y << " " << triangle->vertices[1].y << " " << triangle->vertices[2].y << '\n';
}

void equateTriangles(CanvasTriangle from, CanvasTriangle *to) {
  for (int i = 0; i < 3; i++) {
    to->vertices[i].x = from.vertices[i].x;
    to->vertices[i].y = from.vertices[i].y;
    to->vertices[i].texturePoint = from.vertices[i].texturePoint;
  }
  to->colour = from.colour;
}

void getTopBottomTriangles(CanvasTriangle triangle, CanvasTriangle *top, CanvasTriangle *bottom) {
  if ((triangle.vertices[0].y == triangle.vertices[1].y) || (triangle.vertices[1].y == triangle.vertices[2].y)) {
    CanvasPoint temp = triangle.vertices[1];
    triangle.vertices[1] = triangle.vertices[2];
    triangle.vertices[2] = temp;
    equateTriangles(triangle, top);
    equateTriangles(triangle, bottom);
    //std::cout << "detected a flat triangle!" << '\n';
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

  //std::cout << "side1.size() " << side1.size() << " side2.size() " << side2.size() << "\n";
  uint last_drawn_y = round(side1.at(0).y);

  if (textured) drawTexturedLine(side1.at(0), side2.at(0));
  else drawLine(side1.at(0), side2.at(0), triangle.colour);

  for (uint i = 0; i < side1.size(); i++) {
    if (round(side1.at(i).y) != last_drawn_y) {
      int j = 0;
      while ((j < (int)side2.size() - 1) && (round(side2.at(j).y) <= last_drawn_y)) {
	       j++;
      }
      //std::cout << "i " << round(side1.at(i).y) << " j " << round(side2.at(j).y);
      if (textured) drawTexturedLine(side1.at(i), side2.at(j));
      else drawLine(side1.at(i), side2.at(j), triangle.colour);
      //std::cout << " DRAWN" << '\n';
      last_drawn_y++;
    }
  }
}

void drawFilledTriangle(CanvasTriangle triangle, bool textured) {
  sortTrianglePoints(&triangle);
  CanvasTriangle triangles[2];
  getTopBottomTriangles(triangle, &triangles[0], &triangles[1]);

  fillFlatBaseTriangle(triangles[0], 0, 1, 0, 2, textured);
  fillFlatBaseTriangle(triangles[1], 0, 1, 2, 1, textured);
}

void drawTextureMappedTriangle() {
  CanvasPoint p1(160, 10);
  CanvasPoint p2(300, 230);
  CanvasPoint p3(10, 150);
  p1.texturePoint = TexturePoint(195, 5);
  p2.texturePoint = TexturePoint(395, 380);
  p3.texturePoint = TexturePoint(65, 330);

  CanvasTriangle triangle(p1, p2, p3);
  std::cout << "p1.tp.x: " << p1.texturePoint.x << " p1.tp.y: " << p1.texturePoint.y << '\n';
  std::cout << "p2.tp.x: " << p2.texturePoint.x << " p2.tp.y: " << p2.texturePoint.y << '\n';
  std::cout << "p3.tp.x: " << p3.texturePoint.x << " p3.tp.y: " << p3.texturePoint.y << '\n';

  drawFilledTriangle(triangle, true);
}

void drawRandomTriangle(bool filled) {
  CanvasTriangle triangle;
  generateTriangle(&triangle);
  if (filled) drawFilledTriangle(triangle, false);
  else drawStrokedTriangle(triangle);
}

uint32_t* loadPPM(string imageName, int* the_width, int* the_height) {
  FILE* f = fopen(imageName.c_str(), "r");
  char buf[64];
  int width, height, maxcolour;

  if (f == NULL) {
    std::cout << "Could not open file." << '\n';
    exit(1);
  }

    fgets(buf, 64, f); // P6
    fgets(buf, 64, f); // could be GIMP comment
    if (buf[0] == '#') {
      fgets(buf, 64, f); // definitely the dimensions
    }
    sscanf(buf, "%d %d", &width, &height);
    fgets(buf, 64, f); // bit depth, should be 255
    sscanf(buf, "%d", &maxcolour);
    if (maxcolour != 255) {
      std::cout << "Unsupported bit depth. Use 255 plz." << '\n';
      exit(1);
    }
    uint8_t linebuf[3 * width];

    uint32_t* ppm_image = (uint32_t*)malloc(width*height*sizeof(uint32_t));

    int y = 0;
    while(!feof(f)) {
      fread(&linebuf, 1, 3*width, f);
      for (int i = 0; i < width; i++) {
        uint32_t colour = (linebuf[3*i] << 16) + (linebuf[3*i + 1] << 8) + linebuf[3*i + 2];
        ppm_image[y*width + i] = colour;
        //window.setPixelColour(i, y, colour);
      }
      y++;
    }
    fclose(f);
    *the_width = width;
    *the_height = height;
    return ppm_image;
}

bool isemptyline(char* line) {
  return line[0] == '\n';
}

void loadMTL(string filename) {
  FILE* f = fopen(filename.c_str(), "r");
  int bufsize = 200;
  char buf[bufsize];

  unordered_map<string, Colour> materials;

  if (f == NULL) {
    std::cout << "Could not open file." << '\n';
    exit(1);
  }

  std::string* tokens;

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
  std::cout << "size of hashmap: " << materials.size() << '\n';
}

void clearScreen() {
  //Draws a white empty screen
  window.clearPixels();

  for(int y=0; y<window.height ;y++) {
    for(int x=0; x<window.width ;x++) {
      uint32_t colour = (0 << 24) + (255 << 16) + (255 << 8) + 255;
      window.setPixelColour(x, y, colour);
    }
  }
}

void draw() {
  clearScreen();
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
    else if(event.key.keysym.sym == SDLK_t) {
      cout << "T: DRAWING TEXTUREMAPPED TRIANGLE" << endl;
      drawTextureMappedTriangle();
    }
    else if(event.key.keysym.sym == SDLK_f) {
      //clearScreen();
      //CanvasTriangle triangle(CanvasPoint(448, 318), CanvasPoint(406, 292), CanvasPoint(40, 172), Colour(255, 0, 0));
      cout << "F: DRAWING RANDOM FILLED TRIANGLE" << endl;
      drawRandomTriangle(true);
    }
    else if(event.key.keysym.sym == SDLK_s) {
      cout << "C: DRAWING RANDOM STROKED TRIANGLE" << endl;
      drawRandomTriangle(false);
    }
    else if(event.key.keysym.sym == SDLK_c) {
      cout << "C: CLEARING SCREEN" << endl;
      clearScreen();
    }
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;

}

int main(int argc, char* argv[]) {
  srand((unsigned) time(0));
  SDL_Event event;
  draw();

  the_image = loadPPM("texture.ppm", &the_image_width, &the_image_height);
  loadMTL("cornell-box.mtl");

  while(true)
  {

    // We MUST poll for events - otherwise the window will freeze !
    if(window.pollForInputEvents(&event)) handleEvent(event);

    update();
    //draw();
    // Need to render the frame at the end, or nothing actually gets shown on the screen !
    window.renderFrame();
  }
}
