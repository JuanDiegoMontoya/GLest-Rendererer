#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Mesh.h"
#include "Object.h"
#include "Light.h"
#include "Camera.h"
#include "Texture.h"

#include <vector>
#include <memory>

#define MULTISAMPLE_TRICK 0


class Renderer
{
public:
  void run();

private:
  void InitWindow();

  void InitGL();

  void InitImGui();

  void MainLoop();

  void CreateFramebuffers();

  void CreateVAO();
  
  void CreateScene();

  void Cleanup();

  void DrawUI();

  void ApplyTonemapping(float dt);

  // common
  GLFWwindow* window{};
  const uint32_t WIDTH = 1440;
  const uint32_t HEIGHT = 810;
  GLuint vao{};
  bool cursorVisible = false;

  // scene info
  Camera cam;
  std::vector<PointLight> localLights;
  std::vector<Mesh> terrainMeshes;
  Mesh sphere;
  std::vector<Object> objects;
  GLuint lightSSBO{};
  float sunPosition = 0;
  DirLight globalLight;

  // volumetric stuff
  std::unique_ptr<Texture2D> bluenoiseTex{};
  GLuint volumetricsFbo{}, volumetricsTex{}, volumetricsTexBlur{};
  int VOLUMETRIC_BLUR_PASSES = 1;
  int VOLUMETRIC_BLUR_STRENGTH = 3;
  const uint32_t VOLUMETRIC_WIDTH = WIDTH / 1;
  const uint32_t VOLUMETRIC_HEIGHT = HEIGHT / 1;
  GLint volumetric_steps = 50;
  float volumetric_intensity = .02f;
  float volumetric_distToFull = 20.0f;
  float volumetric_noiseOffset = 1.0f;

  // a-trous filter stuff
  GLuint atrousFbo{}, atrousTex{};
  bool volumetric_atrousEnabled = true;
  float c_phi = 1.0f;
  float n_phi = 1.0f;
  float p_phi = 1.0f;
  float stepWidth = 1.0f;
  const float atrouskernel[25] = {
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
  GLuint gfbo{}, gAlbedoSpec{}, gNormal{}, gDepth{}, gShininess{};
  GLuint postprocessFbo{}, postprocessColor{};

  // shadow stuff
  GLuint shadowFbo{}, shadowDepth{}, shadowGoodFormatFbo{}, shadowDepthGoodFormat{};
  GLuint shadowMomentBlur{};
  GLuint shadowTargetFbo{}, shadowDepthTarget{}, shadowDepthSquaredTarget{};
  GLuint SHADOW_WIDTH = 1024;
  GLuint SHADOW_HEIGHT = 1024;
  float lightBleedFix = .9f;
#if MULTISAMPLE_TRICK
  const int NUM_MULTISAMPLES = 4;
#else
  int BLUR_PASSES = 1;
  int BLUR_STRENGTH = 3;
#endif

  // HDR stuff
  GLuint hdrfbo{}, hdrColor{}, hdrDepth{};
  GLuint histogramBuffer{}, exposureBuffer{};
  float targetLuminance = .22f;
  float minExposure = .25f;
  float maxExposure = 20.0f;
  float exposureFactor = 1.0f;
  float adjustmentSpeed = 2.0f;
  const int NUM_BUCKETS = 128;
};