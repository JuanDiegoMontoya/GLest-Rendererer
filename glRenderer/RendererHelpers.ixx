module;

#include <string>
#include <span>
#include <glad/glad.h>
#include <iostream>
#include "Shader.h"

export module RendererHelpers;

export void GLAPIENTRY
  GLerrorCB(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    [[maybe_unused]] const void* userParam)
{
  // ignore non-significant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204
    )//|| id == 131188 || id == 131186)
    return;

  std::cout << "---------------" << std::endl;
  std::cout << "Debug message (" << id << "): " << message << std::endl;

  switch (source)
  {
  case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window Manager"; break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
  case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
  case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
  } std::cout << std::endl;

  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
  case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
  case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
  case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
  case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
  case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
  case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
  } std::cout << std::endl;

  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
  case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
  case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
  case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
  } std::cout << std::endl;
  std::cout << std::endl;
}

export void CompileShaders()
{
  Shader::shaders["gBuffer"].emplace(Shader(
    {
      { "gBuffer.vs", GL_VERTEX_SHADER },
      { "gBuffer.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["gBufferBindless"].emplace(Shader(
    {
      { "gBufferBindless.vs", GL_VERTEX_SHADER },
      { "gBufferBindless.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["fstexture"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "texture.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["fstexture_depth"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "texture_depth.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["gPhongGlobal"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "gPhongGlobal.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["gPhongManyLocal"].emplace(Shader(
    {
      { "lightGeom.vs", GL_VERTEX_SHADER },
      { "gPhongManyLocal.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["generate_histogram"].emplace(Shader(
    { { "generate_histogram.cs", GL_COMPUTE_SHADER } }));
  Shader::shaders["calc_exposure"].emplace(Shader(
    { { "calc_exposure.cs", GL_COMPUTE_SHADER } }));

  Shader::shaders["gaussian_blur6"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 6"}}} }));
  Shader::shaders["gaussian_blur5"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 5"}}} }));
  Shader::shaders["gaussian_blur4"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 4"}}} }));
  Shader::shaders["gaussian_blur3"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 3"}}} }));
  Shader::shaders["gaussian_blur2"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 2"}}} }));
  Shader::shaders["gaussian_blur1"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 1"}}} }));

  Shader::shaders["gaussian32f_blur6"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 6"}, {"#define FORMAT RG32f", "#define FORMAT R32f"}}} }));
  Shader::shaders["gaussian32f_blur5"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 5"}, {"#define FORMAT RG32f", "#define FORMAT R32f"}}} }));
  Shader::shaders["gaussian32f_blur4"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 4"}, {"#define FORMAT RG32f", "#define FORMAT R32f"}}} }));
  Shader::shaders["gaussian32f_blur3"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 3"}, {"#define FORMAT RG32f", "#define FORMAT R32f"}}} }));
  Shader::shaders["gaussian32f_blur2"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 2"}, {"#define FORMAT RG32f", "#define FORMAT R32f"}}} }));
  Shader::shaders["gaussian32f_blur1"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 1"}, {"#define FORMAT RG32f", "#define FORMAT R32f"}}} }));

  Shader::shaders["gaussianRGBA32f_blur6"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 6"}, {"#define FORMAT RG32f", "#define FORMAT RGBA32f"}}} }));
  Shader::shaders["gaussianRGBA32f_blur5"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 5"}, {"#define FORMAT RG32f", "#define FORMAT RGBA32f"}}} }));
  Shader::shaders["gaussianRGBA32f_blur4"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 4"}, {"#define FORMAT RG32f", "#define FORMAT RGBA32f"}}} }));
  Shader::shaders["gaussianRGBA32f_blur3"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 3"}, {"#define FORMAT RG32f", "#define FORMAT RGBA32f"}}} }));
  Shader::shaders["gaussianRGBA32f_blur2"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 2"}, {"#define FORMAT RG32f", "#define FORMAT RGBA32f"}}} }));
  Shader::shaders["gaussianRGBA32f_blur1"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 1"}, {"#define FORMAT RG32f", "#define FORMAT RGBA32f"}}} }));

  Shader::shaders["gaussianRGBA16f_blur6"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 6"}, {"#define FORMAT RG32f", "#define FORMAT RGBA16f"}}} }));
  Shader::shaders["gaussianRGBA16f_blur5"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 5"}, {"#define FORMAT RG32f", "#define FORMAT RGBA16f"}}} }));
  Shader::shaders["gaussianRGBA16f_blur4"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 4"}, {"#define FORMAT RG32f", "#define FORMAT RGBA16f"}}} }));
  Shader::shaders["gaussianRGBA16f_blur3"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 3"}, {"#define FORMAT RG32f", "#define FORMAT RGBA16f"}}} }));
  Shader::shaders["gaussianRGBA16f_blur2"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 2"}, {"#define FORMAT RG32f", "#define FORMAT RGBA16f"}}} }));
  Shader::shaders["gaussianRGBA16f_blur1"].emplace(Shader(
    { { "gaussian.cs", GL_COMPUTE_SHADER, {{"#define KERNEL_RADIUS 3", "#define KERNEL_RADIUS 1"}, {"#define FORMAT RG32f", "#define FORMAT RGBA16f"}}} }));

  Shader::shaders["tonemap"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "tonemap.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["fxaa"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "fxaa.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["shadow"].emplace(Shader(
    { { "shadow.vs", GL_VERTEX_SHADER },
    }));
  Shader::shaders["shadowBindless"].emplace(Shader(
    { { "shadowBindless.vs", GL_VERTEX_SHADER },
    }));
  Shader::shaders["volumetric"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "volumetric.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["vsm_copy"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "vsm_copy.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["esm_copy"].emplace(Shader(
    { { "fullscreen_tri.vs", GL_VERTEX_SHADER },
    { "esm_copy.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["msm_copy"].emplace(Shader(
    { { "fullscreen_tri.vs", GL_VERTEX_SHADER },
    { "msm_copy.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["atrous"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "atrous.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["atrous_volumetric"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "atrous_volumetric.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["atrous_ssao"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "atrous_ssao.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["ssr"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "ssr.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["ssao"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "ssao.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["hdri_skybox"].emplace(Shader(
    {
      { "fullscreen_tri.vs", GL_VERTEX_SHADER },
      { "hdri_skybox.fs", GL_FRAGMENT_SHADER }
    }));
  Shader::shaders["convolve_image"].emplace(Shader(
    { { "irradiance_convolve.cs", GL_COMPUTE_SHADER } }));
}

export void drawFSTexture(GLuint texID)
{
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glBindTextureUnit(0, texID);
  auto& fullscreenTextureShader = Shader::shaders["fstexture"];
  fullscreenTextureShader->Bind();
  fullscreenTextureShader->SetInt("u_texture", 0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

namespace // detail
{
  void blurTextureBase(GLuint inOutTex, GLuint intermediateTexture, GLint width, GLint height,
    GLint passes, GLint strength, std::span<std::string> shaderNames, GLenum imageFormat)
  {
    if (strength > shaderNames.size() || strength <= 1)
    {
      return;
    }

    auto& shader = Shader::shaders[shaderNames[strength - 1]];
    shader->Bind();
    shader->SetIVec2("u_texSize", width, height);
    shader->SetInt("u_inTex", 0);
    shader->SetInt("u_outTex", 0);

    const int X_SIZE = 8;
    const int Y_SIZE = 8;
    const int xgroups = (width + X_SIZE - 1) / X_SIZE;
    const int ygroups = (height + Y_SIZE - 1) / Y_SIZE;

    bool horizontal = false;
    for (int i = 0; i < passes * 2; i++)
    {
      // read from inTex on first pass only
      glBindTextureUnit(0, inOutTex);
      glBindImageTexture(0, intermediateTexture, 0, false, 0, GL_WRITE_ONLY, imageFormat);
      shader->SetBool("u_horizontal", horizontal);
      glDispatchCompute(xgroups, ygroups, 1);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      std::swap(inOutTex, intermediateTexture);
      horizontal = !horizontal;
    }
  }
}

export void blurTextureRGBA32f(GLuint inOutTex, GLuint intermediateTexture, GLint width, GLint height, GLint passes, GLint strength)
{
  std::string strs[] = { "gaussianRGBA32f_blur1","gaussianRGBA32f_blur2", "gaussianRGBA32f_blur3", "gaussianRGBA32f_blur4", "gaussianRGBA32f_blur5", "gaussianRGBA32f_blur6" };
  blurTextureBase(inOutTex, intermediateTexture, width, height, passes, strength, strs, GL_RGBA32F);
}

export void blurTextureRGBA16f(GLuint inOutTex, GLuint intermediateTexture, GLint width, GLint height, GLint passes, GLint strength)
{
  std::string strs[] = { "gaussianRGBA16f_blur1","gaussianRGBA16f_blur2", "gaussianRGBA16f_blur3", "gaussianRGBA16f_blur4", "gaussianRGBA16f_blur5", "gaussianRGBA16f_blur6" };
  blurTextureBase(inOutTex, intermediateTexture, width, height, passes, strength, strs, GL_RGBA16F);
}

export void blurTextureRG32f(GLuint inOutTex, GLuint intermediateTexture, GLint width, GLint height, GLint passes, GLint strength)
{
  std::string strs[] = { "gaussian_blur1","gaussian_blur2", "gaussian_blur3", "gaussian_blur4", "gaussian_blur5", "gaussian_blur6" };
  blurTextureBase(inOutTex, intermediateTexture, width, height, passes, strength, strs, GL_RG32F);
}

export void blurTextureR32f(GLuint inOutTex, GLuint intermediateTexture, GLint width, GLint height, GLint passes, GLint strength)
{
  std::string strs[] = { "gaussian32f_blur1", "gaussian32f_blur2", "gaussian32f_blur3", "gaussian32f_blur4", "gaussian32f_blur5", "gaussian32f_blur6" };
  blurTextureBase(inOutTex, intermediateTexture, width, height, passes, strength, strs, GL_R32F);
}

export void convolve_image(GLuint inTex, GLuint outTex, GLint outWidth, GLint outHeight)
{
  auto& shader = Shader::shaders["convolve_image"];
  shader->Bind();
  shader->SetInt("u_environment", 0);
  shader->SetInt("u_outTex", 0);

  const int X_SIZE = 8;
  const int Y_SIZE = 8;
  const int xgroups = (outWidth + X_SIZE - 1) / X_SIZE;
  const int ygroups = (outHeight + Y_SIZE - 1) / Y_SIZE;

  glBindTextureUnit(0, inTex);
  glBindImageTexture(0, outTex, 0, false, 0, GL_WRITE_ONLY, GL_RGBA16F);
  glDispatchCompute(xgroups, ygroups, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}