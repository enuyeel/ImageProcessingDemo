#if defined (__EMSCRIPTEN__)

#include <emscripten/emscripten.h>
//#include <emscripten/html5_webgl.h>
#include <webgl/webgl2.h>
#include <SDL2/SDL.h>

//#elif defined(_MSC_VER)
#elif defined(_MSC_VER)

#include <SDL2-2.0.20/include/SDL.h>
#include <glew-2.1.0/include/GL/glew.h>
//#include <ImGuiFileDialog/ImGuiFileDialog.h>

#endif

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_opengl3.h>
//#include <implot/implot.h>
//#include <implot/implot_internal.h>
#include <opencv2/opencv.hpp>
//#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp> //glm::value_ptr()

#include <sstream>
#include <iostream>
#include <vector>
#include <fstream> //ifstream
#include <cfloat> //FLT_MAX
#include <algorithm> //clamp, max, swap
#include <queue>
#include <cmath> //log

#include "misc.h"

namespace global {

  SDL_Window* window = 0;
  cv::VideoCapture* pCapture = 0;
  SDL_GLContext context = 0;
  GLsizei windowWidth = 640;
  GLsizei windowHeight = 480;

#if defined(_MSC_VER)
  bool isGLEWInitialized = false;
#endif

  enum objects {
    quad,
    objectCount
  };
  GLuint vertexArrayObjects[objectCount] = { 0 };
  GLuint bufferObjects[objectCount] = { 0 };

  enum programList {
    drawFullscreenQuad,
    scaleImage,
    operations,
    programCount
  };
  GLuint programs[programCount] = { 0 };

  //Quad itself is not a fullscreen size, but it should be scaled accordingly.
  float quadModelSpaceVertices[] =
  {
    0.f, 0.f, 0.f,
    1.f, 0.f, 0.f,
    0.f, 1.f, 0.f,

    1.f, 0.f, 0.f,
    1.f, 1.f, 0.f,
    0.f, 1.f, 0.f

    //0.f, 0.f, 0.f,
    //0.f, 1.f, 0.f,
    //1.f, 0.f, 0.f,

    //1.f, 0.f, 0.f,
    //0.f, 0.f, 0.f,
    //1.f, 1.f, 0.f,
  };

  enum topology {
    four,
    eight,
    m,
    topologyCount
  };

  //float top = 0.001f;
  //float bottom = -0.001f;
  //float right = 0.001f;
  //float left = -0.001f;
  //float zNear = 0.001f;
  //float zFar = 1000.f;

  glm::mat4 mtow(1.0);
  glm::mat4 wtoc(1.0);
  glm::mat4 ctoc(1.0);

  //Original size.
  GLuint sourceTexture = 0;
  //TODO: Better save the dimension of the source texture, since it is not
  //      possible to query for GL_TEXTURE_WIDTH & GL_TEXTURE_HEIGHT on WebGL,
  //      and the dominant use case is limited to the source texture.
  GLint sourceWidth = 0;
  GLint sourceHeight = 0;
  //Upscaled texture.
  GLuint destinationTexture = 0;

  //Textures reserved for operations.
  GLuint reserveTexture1 = 0;
  GLuint reserveTexture2 = 0;
  GLuint reserveTexture3 = 0;
  GLuint reserveTexture4 = 0; //This one is specifically reserved for CCL.

  std::vector<uint8_t> copied;
  GLuint myFramebuffer = 0;
  bool isNearestNeighbor = true;
  enum operations {
    NONE = -1,
    normalization,
    equalization,
    gaussianBlur,
    sobel,
    unsharpMasking
  };
  GLint operationIndex = 0;

  struct PPMImage
  {
    PPMImage() : width(0), height(0)//, L(0), isSample1Byte(true) 
    {};

    bool loadPPM(const char* filePath);

    int width;
    int height;

    //uint32_t L; //maximum pixel value

    //[http://netpbm.sourceforge.net/doc/ppm.html]
    //Each sample is represented in pure binary by either 1 or 2 bytes.
    //If the Maxval is less than 256, it is 1 byte. Otherwise, it is 2 bytes.
    //bool isSample1Byte;

    std::vector<uint8_t> bufferUint8;   //Used if L is 1 byte.
    //std::vector<uint16_t> bufferUint16; //Used if L is 2 bytes.
  };

  std::vector<std::pair<std::string, PPMImage>> ppmImages;
  std::vector<const char*> ppmImagesIdentifiers;

  std::vector<std::pair<uint32_t, uint32_t>> webcamResolutions;
  cv::Mat frame;

  //Meshify
  float interlineDistance = 8.f;
  uint32_t meshify = 0; //GLSL; an unsigned 32-bit integer
  float amplicationFactor = 4.f;
  float meshifyThickness = 6.f;

  //Chromatic Aberration
  uint32_t chromaticAuto = 0;
  float offsetAmount = 0.125f;

  //Gradient Color
  uint32_t paletteIdx = 0;

  float iTime = 0.0;
}

static void initializeEmptyTexture(GLuint destinationTexture, int width, int height)
{
  if (!glIsTexture(destinationTexture))
    return;

  glBindTexture(GL_TEXTURE_2D, destinationTexture);
  opengl_check_error();

  //[https://stackoverflow.com/questions/71284184/opengl-distorted-texture]
  //When a RGB image with 3 color channels is loaded to a texture object and 3 * width is not divisible by 4,
  //GL_UNPACK_ALIGNMENT has to be set to 1, before specifying the texture image with glTexImage2D().
  //if (width * 3 % 4)
  //{
  //  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  //  opengl_check_error();
  //}

  //[https://registry.khronos.org/OpenGL-Refpages/es3.1/html/glTexImage2D.xhtml]
  //TODO: GL_RGB8UI seemed to be the most plausible choice, but it's not color-renderable according
  //      to the Khronos documentation. Debugging the texture showed just outright normalized values
  //      for any pixel value over 0, making them 99% white.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, //Only supporting 256 pixel depth (8 bits) PPM P3 image.
    width, height,
    //global::windowWidth, global::windowHeight, 
    0, GL_RGB,
    GL_UNSIGNED_BYTE, //8 bits
    0);
  opengl_check_error();

  //[https://stackoverflow.com/questions/45613310/switching-from-texture-to-texelfetch]
  //These below 4 are ideal setup for manually implementing bilinear & nearest-neighbor interpolation;
  //display texture as-is (how we expect it)?
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  opengl_check_error();

  glBindTexture(GL_TEXTURE_2D, 0);
  opengl_check_error();
}

static void scaleTexture(GLuint srcTexture, GLuint destTexture, GLsizei width, GLsizei height)
{
  if (!glIsTexture(srcTexture) ||
      !glIsTexture(destTexture))
    return;

  glUseProgram(global::programs[global::scaleImage]);
  opengl_check_error();

  GLint loc = glGetUniformLocation(global::programs[global::scaleImage], "mtow");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::mtow));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::scaleImage], "wtoc");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::wtoc));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::scaleImage], "ctoc");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::ctoc));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::scaleImage], "sourceTexture");
  opengl_check_error();
  glUniform1i(loc, 0);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::scaleImage], "isNearestNeighbor");
  opengl_check_error();
  glUniform1i(loc, global::isNearestNeighbor); //TODO: Filler
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::scaleImage], "fbWidth");
  opengl_check_error();
  glUniform1ui(loc, width);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::scaleImage], "fbHeight");
  opengl_check_error();
  glUniform1ui(loc, height);
  opengl_check_error();

  //glActiveTexture selects which texture unit subsequent texture state calls will affect.
  glActiveTexture(GL_TEXTURE0);
  opengl_check_error();
  glBindTexture(GL_TEXTURE_2D, srcTexture);
  opengl_check_error();

  glActiveTexture(GL_TEXTURE0 + 1);
  opengl_check_error();
  glBindTexture(GL_TEXTURE_2D, destTexture);
  opengl_check_error();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, //Only supporting 256 pixel depth (8 bits) PPM P3 image.
    width, height,
    //global::windowWidth, global::windowHeight, 
    0, GL_RGB,
    GL_UNSIGNED_BYTE, //8 bits
    0);
  opengl_check_error();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  opengl_check_error();

  glBindFramebuffer(GL_FRAMEBUFFER, global::myFramebuffer);
  opengl_check_error();

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTexture, 0);
  opengl_check_error();

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
  {
    glViewport(0, 0, width, height);
    opengl_check_error();
    glBindVertexArray(global::vertexArrayObjects[global::quad]);
    opengl_check_error();
    glDrawArrays(GL_TRIANGLES, 0, (sizeof(global::quadModelSpaceVertices) / sizeof(global::quadModelSpaceVertices[0])) / 3);
    opengl_check_error();
  }

  glBindVertexArray(0);
  opengl_check_error();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  opengl_check_error();

  glUseProgram(0);
  opengl_check_error();
}

static void drawFullscreenQuad(GLuint srcTexture)
{
  if(!glIsTexture(srcTexture))
    return;

  glUseProgram(global::programs[global::drawFullscreenQuad]);
  opengl_check_error();

  GLint loc = glGetUniformLocation(global::programs[global::drawFullscreenQuad], "mtow");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::mtow));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::drawFullscreenQuad], "wtoc");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::wtoc));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::drawFullscreenQuad], "ctoc");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::ctoc));
  opengl_check_error();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, srcTexture);
  opengl_check_error();
  loc = glGetUniformLocation(global::programs[global::drawFullscreenQuad], "sourceTexture");
  opengl_check_error();
  glUniform1i(loc, 0);
  opengl_check_error();

  glBindVertexArray(global::vertexArrayObjects[global::quad]);
  opengl_check_error();
  glDrawArrays(GL_TRIANGLES, 0, (sizeof(global::quadModelSpaceVertices) / sizeof(global::quadModelSpaceVertices[0])) / 3);
  opengl_check_error();
  glBindVertexArray(0);
  opengl_check_error();

  glUseProgram(0);
  opengl_check_error();
}

static std::vector<char> readFile(const std::string& filename)
{
  std::ifstream file(filename, std::ios::ate);

  if (!file.is_open())
    ERROR_MESSAGE("Failed to open given file.");

  //std::streampos length = file.tellg();
  if (file.tellg() == -1)
    ERROR_MESSAGE("Failed to retrieve the current position for a file.");

  //TODO: Pretty sure we can cast it to 32bit unsigned integer, knowing that the return type for tellg() is a signed integer.
  std::streamoff length = file.tellg();
  //* glShaderSource(), which is later called expects the buffer to be null terminated,
  //* when its last(4th) parameter is null(0), hence + 1 to the actual length.
  std::vector<char> buffer(static_cast<size_t>(length + 1));
  std::fill(buffer.begin(), buffer.end(), '\0');

  file.seekg(0, file.beg);
  file.read(buffer.data(), length);

  file.close();

  return buffer;
}

static bool loadShader(GLuint& shaderHandle, const char* path, GLenum type)
{
  std::vector<char> buffer = readFile(path);
  //std::cout << buffer.data() << std::endl;

  shaderHandle = glCreateShader(type);
  opengl_check_error(); //GL_INVALID_ENUM is generated if shaderType is not an accepted value.
  if (!shaderHandle)
    return false;

  const GLchar* string = buffer.data();

  //If length is NULL, each string is assumed to be null terminated.
  glShaderSource(shaderHandle, 1, &string, 0);
  opengl_check_error();

  glCompileShader(shaderHandle);
  opengl_check_error();

  GLint compileStatus;
  glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileStatus);
  opengl_check_error();

  if (compileStatus != GL_TRUE)
  {
#if defined(_DEBUG) && (_MSC_VER) || defined (__EMSCRIPTEN__)
    GLint logLength;
    glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &logLength);
    opengl_check_error();
    std::string compilationLog(logLength, '\0');
    glGetShaderInfoLog(shaderHandle, logLength, 0, &(compilationLog[0]));
    opengl_check_error();
    compilationLog.insert(0, "Shader compilation failure\n");
    glDeleteShader(shaderHandle);

#if defined (__EMSCRIPTEN__)
    printf("%s\n", compilationLog.data());
#endif
    ERROR_MESSAGE(compilationLog.c_str());
#endif

    return false;
  }

  return true;
}

static bool linkProgram(GLuint program)
{
  glLinkProgram(program);
  opengl_check_error();

  GLint linkStatus;
  glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
  opengl_check_error();

  if (linkStatus != GL_TRUE)
  {
#if ( defined (_DEBUG) && (_MSC_VER) ) || defined (__EMSCRIPTEN__)
    GLint logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    opengl_check_error();
    std::string linkLog(logLength, '\0');
    glGetProgramInfoLog(program, logLength, 0, &linkLog[0]);
    opengl_check_error();
    linkLog.insert(0, "Shaders link failure.\n");

#if defined (__EMSCRIPTEN__)
    printf("%s\n", linkLog.data());
#endif
    ERROR_MESSAGE(linkLog.data());
#endif
    return false;
  }

  return true;
}

static void ImGuiDraw()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  ImGui::NewFrame();

  static bool isDockspaceOpen = true;

  if (isDockspaceOpen)
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

  static bool isPPMImagesWindowOpen = true;
  ImGui::Begin("Toolbar", &isPPMImagesWindowOpen);

  ImGui::Checkbox("Show Dockspace", &isDockspaceOpen);

  if (ImGui::CollapsingHeader("Image Operations"))
  {
    if (ImGui::Button("Default"))
      global::operationIndex = 0;  
    ImGui::Separator();

    if (ImGui::Button("Negative"))
      global::operationIndex = 1;
    ImGui::Separator();

    if (ImGui::Button("Edge Detection"))
      global::operationIndex = 2;
    ImGui::Separator();

    if (ImGui::Button("Chromatic Aberration"))
      global::operationIndex = 3;
    ImGui::DragFloat("Offset", &global::offsetAmount, 0.01f, 0.f, 1.f);
    if (ImGui::Selectable("Manual", global::chromaticAuto == 0))
      global::chromaticAuto = 0;
    if (ImGui::Selectable("Auto", global::chromaticAuto == 1))
      global::chromaticAuto = 1;
    ImGui::Separator();

    if (ImGui::Button("Pixellation"))
      global::operationIndex = 4;
    ImGui::Separator();

    if (ImGui::Button("Water"))
      global::operationIndex = 5;
    ImGui::Separator();

    if (ImGui::Button("Meshify"))
      global::operationIndex = 6;
    ImGui::DragFloat("Interline Distance", &global::interlineDistance, 0.1f, 5.f, 20.f);
    ImGui::DragFloat("Amplication Factor", &global::amplicationFactor, 0.1f, 1.f, 10.f);
    ImGui::DragFloat("Thickness", &global::meshifyThickness, 0.1f, 1.f, 10.f);
    if (ImGui::Selectable("Horizontal", global::meshify == 0))
      global::meshify = 0;
    if (ImGui::Selectable("Vertical", global::meshify == 1))
      global::meshify = 1;
    if (ImGui::Selectable("Horizontal & Vertical", global::meshify == 2))
      global::meshify = 2;
    ImGui::Separator();

    if (ImGui::Button("Gradient Color"))
      global::operationIndex = 7;
    if (ImGui::Selectable("Palette 0", global::paletteIdx == 0))
      global::paletteIdx = 0;
    if (ImGui::Selectable("Palette 1", global::paletteIdx == 1))
      global::paletteIdx = 1;
    if (ImGui::Selectable("Palette 2", global::paletteIdx == 2))
      global::paletteIdx = 2;
    if (ImGui::Selectable("Palette 3", global::paletteIdx == 3))
      global::paletteIdx = 3;
    if (ImGui::Selectable("Palette 4", global::paletteIdx == 4))
      global::paletteIdx = 4;
    if (ImGui::Selectable("Palette 5", global::paletteIdx == 5))
      global::paletteIdx = 5;
    if (ImGui::Selectable("Palette 6", global::paletteIdx == 6))
      global::paletteIdx = 6;
    if (ImGui::Selectable("Palette 7", global::paletteIdx == 7))
      global::paletteIdx = 7;
    if (ImGui::Selectable("Palette 8", global::paletteIdx == 8))
      global::paletteIdx = 8;
    if (ImGui::Selectable("Palette 9", global::paletteIdx == 9))
      global::paletteIdx = 9;
    if (ImGui::Selectable("Palette 10", global::paletteIdx == 10))
      global::paletteIdx = 10;
    if (ImGui::Selectable("Palette 11", global::paletteIdx == 11))
      global::paletteIdx = 11;
    ImGui::Separator();
  }


  ImGui::End();

  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void readSourceTexture(GLuint srcTexture, GLuint destTexture)
{
    glBindTexture(GL_TEXTURE_2D, srcTexture);
    opengl_check_error();

    global::copied.resize(global::sourceWidth * global::sourceHeight * 3);

    //[https://stackoverflow.com/questions/26046619/glgetteximage-reads-too-much-data-with-texture-format-gl-alpha]
    //When OpenGL returns (seemingly) more pixels than it has to. 
    if (global::sourceWidth * 3 % 4)
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        opengl_check_error();
    }

    //TODO: Mapping 3 components to 1 component when reading from texture?
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, global::copied.data());
    opengl_check_error();

    glBindTexture(GL_TEXTURE_2D, destTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, //Only supporting 256 pixel depth (8 bits) PPM P3 image.
        global::sourceWidth, global::sourceHeight,
        0, GL_RGB,
        GL_UNSIGNED_BYTE, //8 bits
        global::copied.data());
}

//Should be called after both source textures are resized to equal sizes.
static void operationsUber(GLuint srcTexture1, GLuint srcTexture2, GLuint destTexture)
{
  if (!glIsTexture(srcTexture1) ||
      !glIsTexture(destTexture))
    return;

  glUseProgram(global::programs[global::operations]);
  opengl_check_error();

  GLint loc = glGetUniformLocation(global::programs[global::operations], "mtow");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::mtow));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "wtoc");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::wtoc));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "ctoc");
  opengl_check_error();
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(global::ctoc));
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "sourceTexture1");
  opengl_check_error();
  glUniform1i(loc, 0);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "sourceTexture2");
  opengl_check_error();
  glUniform1i(loc, 1);
  opengl_check_error();

  //uniform int operations;
  loc = glGetUniformLocation(global::programs[global::operations], "operationIndex");
  opengl_check_error();
  glUniform1i(loc, global::operationIndex);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "interlineDistance");
  opengl_check_error();
  glUniform1f(loc, global::interlineDistance);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "meshify");
  opengl_check_error();
  glUniform1ui(loc, global::meshify);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "chromaticAuto");
  opengl_check_error();
  glUniform1ui(loc, global::chromaticAuto);
  opengl_check_error();


  loc = glGetUniformLocation(global::programs[global::operations], "amplicationFactor");
  opengl_check_error();
  glUniform1f(loc, global::amplicationFactor);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "meshifyThickness");
  opengl_check_error();
  glUniform1f(loc, global::meshifyThickness);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "offsetAmount");
  opengl_check_error();
  glUniform1f(loc, global::offsetAmount);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "iTime");
  opengl_check_error();
  glUniform1f(loc, global::iTime);
  opengl_check_error();

  loc = glGetUniformLocation(global::programs[global::operations], "paletteIdx");
  opengl_check_error();
  glUniform1ui(loc, global::paletteIdx);
  opengl_check_error();

  //glActiveTexture selects which texture unit subsequent texture state calls will affect.
  glActiveTexture(GL_TEXTURE0);
  opengl_check_error();
  glBindTexture(GL_TEXTURE_2D, srcTexture1);
  opengl_check_error();

  //readSourceTexture(global::reserveTexture2, global::reserveTexture1);
  glActiveTexture(GL_TEXTURE0 + 1);
  opengl_check_error();
  glBindTexture(GL_TEXTURE_2D, destTexture);
  opengl_check_error();

  glActiveTexture(GL_TEXTURE0 + 2);
  opengl_check_error();
  glBindTexture(GL_TEXTURE_2D, destTexture);
  opengl_check_error();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  opengl_check_error();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  opengl_check_error();

  glBindFramebuffer(GL_FRAMEBUFFER, global::myFramebuffer);
  opengl_check_error();

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTexture, 0);
  opengl_check_error();

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    glUseProgram(0);
    opengl_check_error();
    return;
  }

  glViewport(0, 0, global::sourceWidth, global::sourceHeight);
  opengl_check_error();

  glBindVertexArray(global::vertexArrayObjects[global::quad]);
  opengl_check_error();
  glDrawArrays(GL_TRIANGLES, 0, (sizeof(global::quadModelSpaceVertices) / sizeof(global::quadModelSpaceVertices[0])) / 3);
  opengl_check_error();
  glBindVertexArray(0);
  opengl_check_error();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  opengl_check_error();

  glUseProgram(0);
  opengl_check_error();
}
static void draw(double deltaTime)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  opengl_check_error();

  global::pCapture->read(global::frame);
  if (!global::frame.empty())
  {
    //int width = frame.cols;
    //int height = frame.rows;
    //int channels = frame.channels();

    //if (!global::sourceWidth || !global::sourceHeight ||
    //    width != global::sourceWidth || height != global::sourceHeight)
    //{
    glBindTexture(GL_TEXTURE_2D, global::sourceTexture);
    opengl_check_error();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,
      global::sourceWidth, global::sourceHeight, 0, 
      //[https://gist.github.com/bigfacebear/b05762180b1ca44047bbb8024c0c7e95]
      //Good catch on how OpenCV stores RGB backwards by default.
      GL_BGR,
      GL_UNSIGNED_BYTE,
      global::frame.data);
    opengl_check_error();

    glBindTexture(GL_TEXTURE_2D, 0);
    opengl_check_error();

    ///////////////OPERATIONS///////////////
    operationsUber(global::sourceTexture, global::reserveTexture1, global::reserveTexture2);

    //readSourceTexture(global::reserveTexture2, global::reserveTexture1);
    scaleTexture(global::reserveTexture2, global::destinationTexture, global::windowWidth, global::windowHeight);

    drawFullscreenQuad(global::destinationTexture);
  }

  //* Actual drawing goes here...
  //drawFullscreenQuad(global::destinationTexture);

  ImGuiDraw();
}

static bool poll()
{
  SDL_Event event;

  // Each loop we will process any events that are waiting for us.
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSDL2_ProcessEvent(&event);

    switch (event.type)
    {
      /*
        Uint32 / type / event type, shared with all events
        SDL_MouseMotionEvent / motion / mouse motion event data
        SDL_MouseButtonEvent / button / mouse button event data
      */
      case SDL_MOUSEBUTTONDOWN:
        break;
        
      case SDL_MOUSEBUTTONUP:
        if (ImGui::GetIO().WantCaptureMouse)
          break;
        break;

      case SDL_MOUSEMOTION:
        break;

      case SDL_QUIT:
        return false;
        break;

      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE)
          return false;
        break;

      case SDL_WINDOWEVENT:
        if (event.window.windowID == SDL_GetWindowID(global::window))
        {
          switch (event.window.event)
          {
            case SDL_WINDOWEVENT_CLOSE:
              return false;
              break;

            case SDL_WINDOWEVENT_SIZE_CHANGED:
              SDL_GL_GetDrawableSize(global::window, &global::windowWidth, &global::windowHeight);
              glViewport(0, 0, global::windowWidth, global::windowHeight);
              opengl_check_error();
              break;
          }
        }
        break;

      default:
        break;
    }
  }

  return true;
}

static void loop(void* arg)
{
  Uint64 now = SDL_GetPerformanceCounter();
  static Uint64 last = 0;

#if defined (__EMSCRIPTEN__)
  poll();
#elif defined(_MSC_VER)
  while (poll())
  {
#endif

    double deltaTime = 0.0;

    last = now;
    now = SDL_GetPerformanceCounter();

    deltaTime = (double)((now - last) * 1000 / (double)SDL_GetPerformanceFrequency() );

    global::iTime += deltaTime;

    draw(deltaTime);

    //Update a window with OpenGL rendering.
    //This is used with double-buffered OpenGL contexts, which are the default.
    SDL_GL_SwapWindow(global::window);

#if not defined (__EMSCRIPTEN__)
  }
#endif

}

//int main() 
int main(int, char* [])
{
#pragma region ContextCreation

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        //https://emscripten.org/docs/porting/Debugging.html#manual-print-debugging
        //printf("Failed to initialize SDL.\nError: %s\n", SDL_GetError());
        std::string msg("Failed to initialize SDL.\nError : ");
        msg.append(SDL_GetError());
#if defined (__EMSCRIPTEN__)
        printf("%s\n", msg.c_str());
#endif
        ERROR_EXIT(msg.c_str());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);

#if defined (__EMSCRIPTEN__)

    const char* glslVersion = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

#elif defined(_MSC_VER)

    const char* glslVersion = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

#endif

    global::window = SDL_CreateWindow(
        "Graphics Application",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        global::windowWidth,
        global::windowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL
    );

    if (!global::window)
    {
        ERROR_EXIT("Failed to create window.");
        return -1;
    }

    // SDL_GL_DeleteContext(context);
    global::context = SDL_GL_CreateContext(global::window);
    if (!global::context)
    {
        ERROR_EXIT("SDL failed to create context.");
        return -1;
    }

#if defined(_MSC_VER)

    if (glewInit() != GLEW_OK)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        ERROR_EXIT("Failed to initialize GLEW.");
        return -1;
    }

#endif

    //* OpenGL functions will draw to window
    SDL_GL_MakeCurrent(global::window, global::context);

    printf("OpenGL Vendor : %s\n", glGetString(GL_VENDOR));
    opengl_check_error();
    printf("OpenGL Renderer : %s\n", glGetString(GL_RENDERER));
    opengl_check_error();
    printf("OpenGL Version : %s\n", glGetString(GL_VERSION));
    opengl_check_error();
    //TODO: This causes exception, of which I can only guess is due to a deprecation, and a possible existence of superceded method of querying extensions string.
    //std::cout << "OpenGL Extensions : " << glGetString(GL_EXTENSIONS) << "\n";
    //opengl_check_error();
    printf("OpenGL Shader Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    opengl_check_error();

#pragma endregion

#pragma region ImGUIInit

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#if defined (__EMSCRIPTEN__)
    io.IniFilename = NULL;
    //const char* ini ="[Window][DockSpaceViewport_11111111]\nPos = 0,0\nSize = 640,480\nCollapsed = 0\n\n[Window][Toolbar]\nPos = 0,0\nSize = 274,480\nCollapsed = 0\nDockId = 0x00000001,0\n\n[Window][Debug##Default]\nPos = 60,60\nSize = 400,400\r\nCollapsed = 0\r\n\r\n[Docking][Data]\r\nDockSpace   ID = 0x8B93E3BD Window = 0xA787BDB4 Pos = 0,0 Size = 640,480 Split = X\r\nDockNode  ID = 0x00000001 Parent = 0x8B93E3BD SizeRef = 274,480 Selected = 0x738351EE\r\nDockNode  ID = 0x00000002 Parent = 0x8B93E3BD SizeRef = 364,480 CentralNode = 1\r\n\r\n";
    //ImGui::LoadIniSettingsFromMemory(ini);
#endif
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(global::window, global::context);
    ImGui_ImplOpenGL3_Init(glslVersion);

    //ImPlot::CreateContext();

#pragma endregion

#pragma region Initialization

  //glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    opengl_check_error();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    opengl_check_error();
    //[https://stackoverflow.com/questions/59634622/game-displayed-in-the-bottom-left-quarter-of-my-screen-in-sdl2]
    SDL_GL_GetDrawableSize(global::window, &global::windowWidth, &global::windowHeight);
    glViewport(0, 0, global::windowWidth, global::windowHeight);
    opengl_check_error();

    cv::VideoCapture capture(0, cv::CAP_ANY);
    if (!capture.isOpened())
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        ERROR_EXIT("Failed to initialize Video Capture.");
        return -1;
    }
    global::pCapture = &capture;
    global::sourceWidth = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    global::sourceHeight = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    global::webcamResolutions.push_back(std::pair(global::sourceWidth, global::sourceHeight));
    global::frame = cv::Mat(global::sourceHeight, global::sourceWidth, CV_8UC3);


    uint32_t resolutions[] = {
      1920, 1080, 1600, 900, 1366, 768, 1280, 720, //16:9
      1280, 960, 1024, 768, 800, 600, 640, 480 //4:3
    };
    for (uint32_t i = 0; i < (sizeof(resolutions) / sizeof(resolutions[0])) / 2; ++i)
    {
        uint32_t width = resolutions[i * 2];
        uint32_t height = resolutions[i * 2 + 1];

        capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);

        width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
        height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

        bool isExist = false;
        for (std::vector<std::pair<uint32_t, uint32_t>>::iterator i = global::webcamResolutions.begin();
            i != global::webcamResolutions.end(); ++i)
        {
            if ((*i).first == width && (*i).second == height)
            {
                isExist = true;
                break;
            }
        }
        if (!isExist) global::webcamResolutions.push_back(std::pair(width, height));
    }

    global::mtow = glm::translate(global::mtow, glm::vec3(-1.0, -1.0, -1.0));
    global::mtow = glm::scale(global::mtow, glm::vec3(2.0, 2.0, 1.0));
    //
    //global::mtow = glm::transpose(global::mtow);
    //global::wtoc = glm::frustum(global::left, global::right, global::bottom, global::top, global::zNear, global::zFar);

    glGenVertexArrays(global::objectCount, global::vertexArrayObjects);
    opengl_check_error();
    glGenBuffers(global::objectCount, global::bufferObjects);
    opengl_check_error();
    glBindVertexArray(global::vertexArrayObjects[global::objects::quad]);
    opengl_check_error();
    glBindBuffer(GL_ARRAY_BUFFER, global::bufferObjects[global::objects::quad]);
    opengl_check_error();
    //void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    glBufferData(GL_ARRAY_BUFFER, sizeof(global::quadModelSpaceVertices), global::quadModelSpaceVertices, GL_STATIC_DRAW);
    opengl_check_error();
    glEnableVertexAttribArray(0);
    opengl_check_error();
    glVertexAttribPointer(0, //GLuint index: Index of the generic vertex attribute to be modified.
                             //              Should be tied to the param of glEnableVertexAttribArray()?
        3, //GLint size:   Number of components per generic vertex attribute.
           //              (x, y, z) position in this particular situation.
        GL_FLOAT, //GLenum type: Data type of each component.
                  //             We're passing 3 float per vertex attribute (position).
        GL_FALSE,//* GLboolean normalized : Integers are to be mapped to the range [-1,1] or [0,1] when are accessed and converted to floating point
        0, //* GLsizei stride
        0);//* const void * pointer : offset of the first component
    opengl_check_error();

    glGenFramebuffers(1, &global::myFramebuffer);
    opengl_check_error();

    glGenTextures(1, &global::sourceTexture);
    opengl_check_error();
    //[https://registry.khronos.org/OpenGL-Refpages/es1.1/xhtml/glIsTexture.xml]
    //A name returned by glGenTextures, but not yet associated with a texture by calling glBindTexture, is not the name of a texture.
    glBindTexture(GL_TEXTURE_2D, global::sourceTexture);
    opengl_check_error();
    initializeEmptyTexture(global::sourceTexture, global::sourceWidth, global::sourceHeight);

    glGenTextures(1, &global::destinationTexture);
    opengl_check_error();
    glBindTexture(GL_TEXTURE_2D, global::destinationTexture);
    opengl_check_error();

    glGenTextures(1, &global::reserveTexture1);
    opengl_check_error();
    glBindTexture(GL_TEXTURE_2D, global::reserveTexture1);
    opengl_check_error();

    glGenTextures(1, &global::reserveTexture2);
    opengl_check_error();
    glBindTexture(GL_TEXTURE_2D, global::reserveTexture2);
    opengl_check_error();

    glGenTextures(1, &global::reserveTexture3);
    opengl_check_error();
    glBindTexture(GL_TEXTURE_2D, global::reserveTexture3);
    opengl_check_error();

    glGenTextures(1, &global::reserveTexture4);
    opengl_check_error();
    glBindTexture(GL_TEXTURE_2D, global::reserveTexture4);
    opengl_check_error();

    glBindTexture(GL_TEXTURE_2D, 0);
    opengl_check_error();

    glBindTexture(GL_TEXTURE_2D, global::reserveTexture2);
    opengl_check_error();
    //No matter what the source textures' formats are, reduce the output texture to 8 bit texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, //Only supporting 256 pixel depth (8 bits) PPM P3 image.
        global::sourceWidth, global::sourceHeight,
        //global::windowWidth, global::windowHeight, 
        0, GL_RGB,
        GL_UNSIGNED_BYTE, //8 bits
        0);
    opengl_check_error();

    global::programs[global::drawFullscreenQuad] = glCreateProgram();
    opengl_check_error();
    global::programs[global::scaleImage] = glCreateProgram();
    opengl_check_error();
    global::programs[global::operations] = glCreateProgram();
    opengl_check_error();
    if (!global::programs[global::drawFullscreenQuad] ||
        !global::programs[global::scaleImage] ||
        !global::programs[global::operations])
    {
        ERROR_EXIT("Failed to create programs.");
        return -1;
    }

    GLuint vertexShaders[global::programCount] = { 0 };
    if (!loadShader(vertexShaders[global::drawFullscreenQuad], "./fullscreenQuad.v", GL_VERTEX_SHADER)
        || !loadShader(vertexShaders[global::scaleImage], "./scaleImage.v", GL_VERTEX_SHADER)
        || !loadShader(vertexShaders[global::operations], "./operations.v", GL_VERTEX_SHADER)
     )
  {
    ERROR_EXIT("Failed to load vertex shaders.");
    return -1;
  }

  GLuint fragmentShaders[global::programCount] = { 0 };
  if (!loadShader(fragmentShaders[global::drawFullscreenQuad], "./fullscreenQuad.f", GL_FRAGMENT_SHADER)
      || !loadShader(fragmentShaders[global::scaleImage], "./scaleImage.f", GL_FRAGMENT_SHADER)
      || !loadShader(fragmentShaders[global::operations], "./operations.f", GL_FRAGMENT_SHADER)
     )
  {
    ERROR_EXIT("Failed to load fragment shaders.");
    return -1;
  }

  glAttachShader(global::programs[global::drawFullscreenQuad], vertexShaders[global::drawFullscreenQuad]);
  opengl_check_error();
  glAttachShader(global::programs[global::drawFullscreenQuad], fragmentShaders[global::drawFullscreenQuad]);
  opengl_check_error();
  if (!linkProgram(global::programs[global::drawFullscreenQuad]))
  {
    ERROR_EXIT("Failed to link programs.");
    return -1;
  }

  glAttachShader(global::programs[global::scaleImage], vertexShaders[global::scaleImage]);
  opengl_check_error();
  glAttachShader(global::programs[global::scaleImage], fragmentShaders[global::scaleImage]);
  opengl_check_error();
  if (!linkProgram(global::programs[global::scaleImage]))
  {
    ERROR_EXIT("Failed to link programs.");
    return -1;
  }

  glAttachShader(global::programs[global::operations], vertexShaders[global::operations]);
  opengl_check_error();
  glAttachShader(global::programs[global::operations], fragmentShaders[global::operations]);
  opengl_check_error();
  if (!linkProgram(global::programs[global::operations]))
  {
      ERROR_EXIT("Failed to link programs.");
      return -1;
  }

#pragma endregion

#pragma region ProjectResourcesInit

#pragma endregion

#if defined (__EMSCRIPTEN__)
  // This function call won't return, and will engage in an infinite loop, processing events from the browser, and dispatching them.
  emscripten_set_main_loop_arg(loop, NULL, 0, true);
#endif

  loop(0);

#pragma endregion

#pragma region Free

#pragma region ImGUIFree

  //ImPlot::DestroyContext();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

#pragma endregion

  glDeleteTextures(1, &global::sourceTexture);
  opengl_check_error();
  glDeleteTextures(1, &global::destinationTexture);
  opengl_check_error();
  glDeleteTextures(1, &global::reserveTexture1);
  opengl_check_error();
  glDeleteTextures(1, &global::reserveTexture2);
  opengl_check_error();
  glDeleteTextures(1, &global::reserveTexture3);
  opengl_check_error();
  glDeleteTextures(1, &global::reserveTexture4);
  opengl_check_error();

  glDeleteVertexArrays(global::objects::objectCount, global::vertexArrayObjects);
  opengl_check_error();

  glDeleteBuffers(global::objects::objectCount, global::bufferObjects);
  opengl_check_error();

  glDeleteFramebuffers(1, &global::myFramebuffer);
  opengl_check_error();

  capture.release();

  SDL_GL_DeleteContext(global::context);
  SDL_DestroyWindow(global::window);
  SDL_Quit();

#pragma endregion

	return 0;

}