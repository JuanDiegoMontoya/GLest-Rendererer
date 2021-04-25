#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

import Mesh;
import Camera;
import RendererHelpers;
import Object;
import Light;
import GPU.Texture;
import GPU.StaticBuffer;
import GPU.DynamicBuffer;

#define SHADOW_METHOD_PCF 0
#define SHADOW_METHOD_VSM 1
#define SHADOW_METHOD_ESM 2
#define SHADOW_METHOD_MSM 3

#define WINDOW_WIDTH 1440
#define WINDOW_HEIGHT 810

class Renderer
{
public:
  void Run();

private:
  void InitWindow();
  void InitGL();
  void InitImGui();
  void MainLoop();
  void Cleanup();

  void CreateFramebuffers();
  void CreateVAO();
  void DrawUI(float dt);
  void ApplyTonemapping(float dt);
  void InitScene();
  void Scene1Lights();
  void Scene2Lights();

  // common
  GLFWwindow* window{};
  GLuint vao{};
  bool cursorVisible = true;
  bool vsyncEnabled{ true };
  float deviceAnisotropy{ 0.0f };

  void LoadScene1();
  void LoadScene2();
  void SetupBuffers(); // draw commands, materials

  // pbr stuff
  std::unique_ptr<Texture2D> envMap_hdri;
  //std::unique_ptr<Texture2D> envMap_irradiance;
  int numEnvSamples{ 10 };
  GLuint irradianceMap{};
  void LoadEnvironmentMap(std::string path);
  void DrawPbrSphereGrid();
  bool drawPbrSphereGridQuestionMark{ false };

  struct SSAOConfig
  {
    bool enabled{ true };
    GLuint fbo{};
    GLuint texture{};
    GLuint textureBlurred{};
    int samples_near{ 12 };
    int samples_mid{ 8 };
    int samples_far{ 4 };
    float near_extent{ 10.0f };
    float mid_extent{ 30.0f };
    float far_extent{ 70.0f };
    float delta{ .001f };
    float range{ 1.1f };
    float s{ 1.8f };
    float k{ 1.0f };
    float atrous_kernel[5] = { 0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f };
    float atrous_offsets[5] = { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f };
    int atrous_passes{ 3 };
    float atrous_n_phi{ .1f };
    float atrous_p_phi{ .5f };
    float atrous_step_width{ 1.0f };
  }ssao;

  // camera
  Camera cam;
  float fovDeg = 80.0f;

  // scene info
  Mesh sphere;
  Mesh sphere2;
  std::vector<ObjectBatched> batchedObjects;
  const int max_vertices{ 5'000'000 };
  std::unique_ptr<DynamicBuffer> vertexBuffer;
  std::unique_ptr<DynamicBuffer> indexBuffer;
  std::unique_ptr<StaticBuffer> materialsBuffer; // material info
  std::unique_ptr<StaticBuffer> drawIndirectBuffer; // DrawElementsIndirectCommand
  MaterialManager materialManager;
  GLuint legitFinalImage{};
  float magnifierScale{ .025f };
  bool magnifierLock{ false };
  glm::vec2 magnifier_lastMouse{};

  // lighting
  std::vector<PointLight> localLights;
  std::unique_ptr<StaticBuffer> lightSSBO;
  float sunPosition{ 0 };
  DirLight globalLight;
  int numLights{ 1000 };
  glm::vec2 lightFalloff{ 2, 8 };
  float lightVolumeThreshold{ 0.01f };
  bool materialOverride{ false };
  glm::vec3 albedoOverride{ 0.129f, 0.643f, 0.921f };
  float roughnessOverride{ 0.5f };
  float metalnessOverride{ 1.0f };
  float ambientOcclusionOverride{ 1.0f };

  struct FXAAConfig
  {
    bool enabled{ true };
    float contrastThreshold{ 0.0312f };
    float relativeThreshold{ 0.125f };
    float pixelBlendStrength{ 1.0f };
    float edgeBlendStrength{ 1.0f };
  }fxaa;

  std::unique_ptr<Texture2D> bluenoiseTex;
  struct VolumetricConfig
  {
    bool enabled{ true };
    GLuint fbo{};
    GLuint tex{};
    GLuint texBlur{};
    GLuint atrousFbo{};
    GLuint atrousTex{};
    int atrous_passes = 1;
    const uint32_t framebuffer_width = WINDOW_WIDTH;
    const uint32_t framebuffer_height = WINDOW_HEIGHT;
    GLint steps{ 20 };
    float intensity{ .1f };
    float distToFull{ 20.0f };
    float noiseOffset{ 1.0f };

    // a-trous filter stuff
    int atrousPasses{ 1 };
    float c_phi{ 0.0001f };
    float stepWidth{ 1.0f };
    const float atrouskernel[25] = { // 5x5 gaussian kernel with std dev=1.75 (I think)
    1.0 / 256.0, 4.0 / 256.0, 6.0 / 256.0, 4.0 / 256.0, 1.0 / 256.0,
    4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0,
    6.0 / 256.0, 24.0 / 256.0, 36.0 / 256.0, 24.0 / 256.0, 6.0 / 256.0,
    4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0,
    1.0 / 256.0, 4.0 / 256.0, 6.0 / 256.0, 4.0 / 256.0, 1.0 / 256.0 };
    const glm::vec2 atrouskerneloffsets[25] = {
      { -2, 2 }, { -1, 2 }, { 0, 2 }, { 1, 2 }, { 2, 2 },
      { -2, 1 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { 2, 1 },
      { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 },
      { -2, -1 }, { -1, -1 }, { 0, -1 }, { 1, -1 }, { 2, -1 },
      { -2, -2 }, { -1, -2 }, { 0, -2 }, { 1, -2 }, { 2, -2 } };
  }volumetrics;

  // deferred stuff
  GLuint gfbo{};
  GLuint gAlbedo{};
  GLuint gNormal{};
  GLuint gDepth{};
  GLuint gRMA{}; // roughness, metalness, ambient occlusion
  GLuint postprocessFbo{};
  GLuint postprocessColor{};
  GLuint postprocessPostSRGB{};

  struct SSRConfig
  {
    bool enabled{ false };
    GLuint fbo{};
    GLuint tex{};
    GLuint texBlur{};
    GLuint framebuffer_width{ WINDOW_WIDTH / 2 };
    GLuint framebuffer_height{ WINDOW_HEIGHT / 2 };
    float rayStep{ 0.15f };
    float minRayStep{ 0.1f };
    float thickness{ 0.0f };
    float searchDist{ 15.0f };
    int maxRaySteps{ 30 };
    int binarySearchSteps{ 5 };
  }ssr;

  // generic shadow stuff
  GLuint shadowFbo{};
  GLuint shadowDepth{};
  GLuint SHADOW_WIDTH{ 1024 };
  GLuint SHADOW_HEIGHT{ 1024 };
  GLuint SHADOW_LEVELS{ (GLuint)glm::ceil(glm::log2((float)glm::max(SHADOW_WIDTH, SHADOW_HEIGHT))) };
  int BLUR_PASSES{ 1 };
  int BLUR_STRENGTH{ 5 };
  int shadow_method{ SHADOW_METHOD_ESM };
  bool shadow_gen_mips{ false };

  // variance shadow stuff
  GLuint vshadowGoodFormatFbo{};
  GLuint vshadowDepthGoodFormat{};
  GLuint vshadowMomentBlur{};
  float vlightBleedFix{ .9f };

  // exponential shadow stuff
  GLuint eShadowFbo{};
  GLuint eExpShadowDepth{};
  GLuint eShadowDepthBlur{};
  float eConstant{ 80.0f };

  // moment shadow stuff
  GLuint msmShadowFbo{};
  GLuint msmShadowMoments{};
  GLuint msmShadowMomentsBlur{};
  float msmA = 3e-5f; // unused

  // HDR stuff
  GLuint hdrfbo{};
  GLuint hdrColor{};
  GLuint hdrDepth{};
  std::unique_ptr<StaticBuffer> histogramBuffer;
  std::unique_ptr<StaticBuffer> exposureBuffer;
  float targetLuminance{ .22f };
  float minExposure{ .1f };
  float maxExposure{ 100.0f };
  float exposureFactor{ 1.0f };
  float adjustmentSpeed{ 2.0f };
  const int NUM_BUCKETS{ 128 };

  GLint uiViewBuffer{};
};