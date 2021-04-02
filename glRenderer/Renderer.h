#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "RendererHelpers.h"
#include "Mesh.h"
#include "Object.h"
#include "Light.h"
#include "Camera.h"
#include "Texture.h"
#include "StaticBuffer.h"
#include "DynamicBuffer.h"

#include <vector>
#include <memory>

#define SHADOW_METHOD_PCF 0
#define SHADOW_METHOD_VSM 1
#define SHADOW_METHOD_ESM 2
#define SHADOW_METHOD_MSM 3

class Renderer
{
public:
  void Run();

private:
  void InitWindow();
  void InitGL();
  void InitImGui();
  void MainLoop();
  void CreateScene();
  void Cleanup();

  void CreateFramebuffers();
  void CreateVAO();
  void DrawUI();
  void ApplyTonemapping(float dt);
  void CreateLocalLights();

  // common
  GLFWwindow* window{};
  const uint32_t WIDTH = 1440;
  const uint32_t HEIGHT = 810;
  //const int FRAMEBUFFER_MULTISAMPLES = 8;
  GLuint vao{};
  bool cursorVisible = true;
  float deviceAnisotropy{ 0.0f };

  void LoadScene1();
  void LoadScene2();
  void SetupBuffers(); // draw commands, materials

  // pbr stuff
  std::unique_ptr<Texture2D> envMap_hdri{};
  std::unique_ptr<Texture2D> envMap_irradiance{};

  // scene info
  Camera cam;
  std::vector<PointLight> localLights;
  Mesh sphere;
  std::vector<ObjectBatched> batchedObjects;
  float sunPosition = 0;
  DirLight globalLight;
  int numLights = 1000;
  glm::vec2 lightFalloff{ 2, 8 };
  const int max_vertices = 5'000'000;
  std::unique_ptr<DynamicBuffer> vertexBuffer;
  std::unique_ptr<DynamicBuffer> indexBuffer;
  std::unique_ptr<StaticBuffer> lightSSBO{};
  std::unique_ptr<StaticBuffer> materialsBuffer; // material info
  std::unique_ptr<StaticBuffer> drawIndirectBuffer; // DrawElementsIndirectCommand
  MaterialManager materialManager;

  // volumetric stuff
  std::unique_ptr<Texture2D> bluenoiseTex{};
  GLuint volumetricsFbo{}, volumetricsTex{}, volumetricsTexBlur{};
  int VOLUMETRIC_BLUR_PASSES = 2;
  int VOLUMETRIC_BLUR_STRENGTH = 2;
  const uint32_t VOLUMETRIC_WIDTH = WIDTH / 1;
  const uint32_t VOLUMETRIC_HEIGHT = HEIGHT / 1;
  GLint volumetric_steps = 50;
  float volumetric_intensity = .02f;
  float volumetric_distToFull = 20.0f;
  float volumetric_noiseOffset = 1.0f;
  bool volumetric_enabled = true;

  // a-trous filter stuff
  GLuint atrousFbo{}, atrousTex{};
  unsigned atrousPasses = 1;
  float c_phi = 0.0001f;
  float n_phi = 1.0f;
  float p_phi = 1.0f;
  float stepWidth = 1.0f;
  const float atrouskernel[25] = { // 5x5 gaussian kernel with std dev=1.75 (I think)
    0.015026f, 0.028569f, 0.035391f, 0.028569f, 0.015026f,
    0.028569f, 0.054318f, 0.067288f, 0.054318f, 0.028569f,
    0.035391f, 0.067288f, 0.083355f, 0.067288f, 0.035391f,
    0.028569f, 0.054318f, 0.067288f, 0.054318f, 0.028569f,
    0.015026f, 0.028569f, 0.035391f, 0.028569f, 0.015026f };
  const glm::vec2 atrouskerneloffsets[25] = {
    { -2, 2 }, { -1, 2 }, { 0, 2 }, { 1, 2 }, { 2, 2 },
    { -2, 1 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { 2, 1 },
    { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 },
    { -2, -1 }, { -1, -1 }, { 0, -1 }, { 1, -1 }, { 2, -1 },
    { -2, -2 }, { -1, -2 }, { 0, -2 }, { 1, -2 }, { 2, -2 } };

  // deferred stuff
  GLuint gfbo{}, gAlbedo{}, gNormal{}, gDepth{}, gRMA{}; // gRMA = roughness, metalness, ambient occlusion
  GLuint postprocessFbo{}, postprocessColor{};

  // ssr stuff
  GLuint ssrFbo{}, ssrTex{}, ssrTexBlur{};
  GLuint SSR_WIDTH = WIDTH / 2;
  GLuint SSR_HEIGHT = HEIGHT / 2;
  float ssr_rayStep = 0.15f;
  float ssr_minRayStep = 0.1f;
  float ssr_thickness = 0.0f;
  float ssr_searchDist = 15.0f;
  int ssr_maxRaySteps = 30;
  int ssr_binarySearchSteps = 5;
  bool ssr_enabled = true;

  // generic shadow stuff
  GLuint shadowFbo{}, shadowDepth{};
  GLuint SHADOW_WIDTH = 1024;
  GLuint SHADOW_HEIGHT = 1024;
  GLuint SHADOW_LEVELS = glm::ceil(glm::log2((float)glm::max(SHADOW_WIDTH, SHADOW_HEIGHT)));
  int BLUR_PASSES = 1;
  int BLUR_STRENGTH = 5;
  int shadow_method = SHADOW_METHOD_ESM;
  bool shadow_gen_mips = false;

  // variance shadow stuff
  GLuint vshadowGoodFormatFbo{}, vshadowDepthGoodFormat{};
  GLuint vshadowMomentBlur{};
  float vlightBleedFix = .9f;

  // exponential shadow stuff
  GLuint eShadowFbo{}, eExpShadowDepth{}, eShadowDepthBlur{};
  float eConstant{ 80.0f };

  // moment shadow stuff
  GLuint msmShadowFbo{}, msmShadowMoments{}, msmShadowMomentsBlur{};
  float msmA = 3e-5f; // unused

  // HDR stuff
  GLuint hdrfbo{}, hdrColor{}, hdrDepth{};
  std::unique_ptr<StaticBuffer> histogramBuffer;
  std::unique_ptr<StaticBuffer> exposureBuffer;
  float targetLuminance = .22f;
  float minExposure = .1f;
  float maxExposure = 100.0f;
  float exposureFactor = 1.0f;
  float adjustmentSpeed = 2.0f;
  const int NUM_BUCKETS = 128;

  GLint uiViewBuffer{};
};