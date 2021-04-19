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
  void DrawUI();
  void ApplyTonemapping(float dt);
  void InitScene();
  void Scene1Lights();
  void Scene2Lights();

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
  std::unique_ptr<Texture2D> envMap_hdri;
  //std::unique_ptr<Texture2D> envMap_irradiance;
  int numEnvSamples = 10;
  GLuint irradianceMap{};
  void LoadEnvironmentMap(std::string path);
  void DrawPbrSphereGrid();
  bool drawPbrSphereGridQuestionMark = false;

  // ssao stuff
  GLuint ssaoFbo{};
  GLuint ambientOcclusionTexture{};
  GLuint ambientOcclusionTextureBlurred{};
  bool ssao_enabled = true;
  int ssao_samples_near{ 10 };
  int ssao_samples_mid{ 10 };
  int ssao_samples_far{ 4 };
  float ssao_near_extent{ 10.0f };
  float ssao_mid_extent{ 30.0f };
  float ssao_far_extent{ 70.0f };
  float ssao_delta{ .001f };
  float ssao_range{ 1.1f };
  float ssao_s{ 1.8f };
  float ssao_k{ 1.0f };
  float ssao_atrous_kernel[13] = { // std dev = 2
    //0.028532f, 0.067234f, 0.124009f, 0.179044f, 0.20236f, 0.179044f, 0.124009f, 0.067234f, 0.028532f };
    0.018816, 0.034474, 0.056577, 0.083173, 0.109523, 0.129188, 0.136498, 0.129188, 0.109523, 0.083173, 0.056577, 0.034474, 0.018816 };
  //float ssao_atrous_offsets[9] = { -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
  float ssao_atrous_offsets[13] = { -6, -5, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5, 6 };
  int ssao_atrous_passes = 2;
  float ssao_atrous_n_phi = 1.0f;
  float ssao_atrous_p_phi = 1.0f;
  float ssao_atrous_step_width = 1.6f;

  // camera
  Camera cam;
  float fovDeg = 80.0f;

  // scene info
  Mesh sphere;
  Mesh sphere2;
  std::vector<ObjectBatched> batchedObjects;
  const int max_vertices = 5'000'000;
  std::unique_ptr<DynamicBuffer> vertexBuffer;
  std::unique_ptr<DynamicBuffer> indexBuffer;
  std::unique_ptr<StaticBuffer> materialsBuffer; // material info
  std::unique_ptr<StaticBuffer> drawIndirectBuffer; // DrawElementsIndirectCommand
  MaterialManager materialManager;

  // lighting
  std::vector<PointLight> localLights;
  std::unique_ptr<StaticBuffer> lightSSBO;
  float sunPosition = 0;
  DirLight globalLight;
  int numLights = 1000;
  glm::vec2 lightFalloff{ 2, 8 };
  float lightVolumeThreshold = 0.01f;
  bool materialOverride = false;
  glm::vec3 albedoOverride{ 0.129f, 0.643f, 0.921f };
  float roughnessOverride{ 0.5f };
  float metalnessOverride{ 1.0f };
  float ambientOcclusionOverride{ 1.0f };

  // volumetric stuff
  std::unique_ptr<Texture2D> bluenoiseTex;
  GLuint volumetricsFbo{}, volumetricsTex{}, volumetricsTexBlur{};
  int VOLUMETRIC_BLUR_PASSES = 1;
  int VOLUMETRIC_BLUR_STRENGTH = 2;
  const uint32_t VOLUMETRIC_WIDTH = WIDTH / 1;
  const uint32_t VOLUMETRIC_HEIGHT = HEIGHT / 1;
  GLint volumetric_steps = 20;
  float volumetric_intensity = .02f;
  float volumetric_distToFull = 20.0f;
  float volumetric_noiseOffset = 1.0f;
  bool volumetric_enabled = true;

  // a-trous filter stuff
  GLuint atrousFbo{}, atrousTex{};
  unsigned atrousPasses = 1;
  float c_phi = 0.0001f;
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
  bool ssr_enabled = false;

  // generic shadow stuff
  GLuint shadowFbo{}, shadowDepth{};
  GLuint SHADOW_WIDTH = 1024;
  GLuint SHADOW_HEIGHT = 1024;
  GLuint SHADOW_LEVELS = (GLuint)glm::ceil(glm::log2((float)glm::max(SHADOW_WIDTH, SHADOW_HEIGHT)));
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