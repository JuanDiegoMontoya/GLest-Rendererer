#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <algorithm>

#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Input.h"
#include "Object.h"
#include "Light.h"

class Timer
{
public:
  Timer() : beg_(clock_::now()) {}
  void reset() { beg_ = clock_::now(); }
  double elapsed() const
  {
    return std::chrono::duration_cast<second_>
      (clock_::now() - beg_).count();
  }

private:
  typedef std::chrono::high_resolution_clock clock_;
  typedef std::chrono::duration<double, std::ratio<1> > second_;
  std::chrono::time_point<clock_> beg_;
};

static thread_local uint64_t x = 123456789, y = 362436069, z = 521288629;

uint64_t xorshf96()
{
  x ^= x << 16;
  x ^= x >> 5;
  x ^= x << 1;

  uint64_t t = x;
  x = y;
  y = z;
  z = t ^ x ^ y;

  return z;
}

double rng()
{
  uint64_t bits = 1023ull << 52ull | xorshf96() & 0xfffffffffffffull;
  return *reinterpret_cast<double*>(&bits) - 1.0;
}

template<typename T, typename Q>
T map(T val, Q r1s, Q r1e, Q r2s, Q r2e)
{
  return (val - r1s) / (r1e - r1s) * (r2e - r2s) + r2s;
}

double rng(double low, double high)
{
  return map(rng(), 0.0, 1.0, low, high);
}

static void GLAPIENTRY
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

class Application
{
public:
  void run()
  {
    initWindow();
    initGL();
    mainLoop();
    cleanup();
  }

private:
  void initWindow()
  {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwSwapInterval(1);

    window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    Input::Init(window);
  }

  void initGL()
  {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
      throw std::exception("Failed to initialize GLAD");
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageCallback(GLerrorCB, nullptr);

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
  }

  void mainLoop()
  {
    // create HDR framebuffer
    glCreateTextures(GL_TEXTURE_2D, 1, &hdrColor);
    glTextureStorage2D(hdrColor, 1, GL_RGBA16F, WIDTH, HEIGHT);
    glCreateTextures(GL_TEXTURE_2D, 1, &hdrDepth);
    glTextureStorage2D(hdrDepth, 1, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT);
    glCreateFramebuffers(1, &hdrfbo);
    glNamedFramebufferTexture(hdrfbo, GL_COLOR_ATTACHMENT0, hdrColor, 0);
    glNamedFramebufferTexture(hdrfbo, GL_DEPTH_ATTACHMENT, hdrDepth, 0);
    if (GLenum status = glCheckNamedFramebufferStatus(hdrfbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
    {
      throw std::runtime_error("Failed to create HDR framebuffer");
    }

    // create tone mapping buffers
    std::vector<int> zeros(NUM_BUCKETS, 0);
    float expo[] = { exposureFactor, 0 };
    glCreateBuffers(1, &exposureBuffer);
    glNamedBufferStorage(exposureBuffer, 2 * sizeof(float), expo, 0);
    glCreateBuffers(1, &histogramBuffer);
    glNamedBufferStorage(histogramBuffer, NUM_BUCKETS * sizeof(int), zeros.data(), 0);

    // create shadow map depth texture and fbo
    glCreateTextures(GL_TEXTURE_2D, 1, &shadowDepth);
    glTextureStorage2D(shadowDepth, 1, GL_DEPTH_COMPONENT32F, SHADOW_WIDTH, SHADOW_HEIGHT);
    const GLfloat txzeros[] = { 0, 0, 0, 0 };
    glTextureParameterfv(shadowDepth, GL_TEXTURE_BORDER_COLOR, txzeros);
    glTextureParameteri(shadowDepth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowDepth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glCreateFramebuffers(1, &shadowFbo);
    glNamedFramebufferTexture(shadowFbo, GL_DEPTH_ATTACHMENT, shadowDepth, 0);
    glNamedFramebufferDrawBuffer(shadowFbo, GL_NONE);
    glNamedFramebufferReadBuffer(shadowFbo, GL_NONE);
    //GLenum shadowFboBuffers[] = { GL_NONE };
    //glNamedFramebufferDrawBuffers(shadowFbo, _countof(shadowFboBuffers), shadowFboBuffers);
    if (GLenum status = glCheckNamedFramebufferStatus(shadowFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
    {
      throw std::runtime_error("Failed to create shadow framebuffer");
    }

    // create texture attachments for gBuffer FBO
    glCreateTextures(GL_TEXTURE_2D, 1, &gPosition);
    glTextureStorage2D(gPosition, 1, GL_RGBA32F, WIDTH, HEIGHT);
    glCreateTextures(GL_TEXTURE_2D, 1, &gAlbedoSpec);
    glTextureStorage2D(gAlbedoSpec, 1, GL_RGBA8, WIDTH, HEIGHT);
    glCreateTextures(GL_TEXTURE_2D, 1, &gNormal);
    glTextureStorage2D(gNormal, 1, GL_RG8_SNORM, WIDTH, HEIGHT);
    //glTextureStorage2D(gNormal, 1, GL_RGBA8_SNORM, WIDTH, HEIGHT);
    glCreateTextures(GL_TEXTURE_2D, 1, &gShininess);
    glTextureStorage2D(gShininess, 1, GL_R16F, WIDTH, HEIGHT);
    glCreateTextures(GL_TEXTURE_2D, 1, &gDepth);
    glTextureStorage2D(gDepth, 1, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT);

    // create gBuffer FBO
    glCreateFramebuffers(1, &gfbo);
    glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT0, gPosition, 0);
    glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT1, gAlbedoSpec, 0);
    glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT2, gNormal, 0);
    glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT3, gShininess, 0);
    glNamedFramebufferTexture(gfbo, GL_DEPTH_ATTACHMENT, gDepth, 0);
    GLenum buffers[] = { GL_COLOR_ATTACHMENT0, 
      GL_COLOR_ATTACHMENT1, 
      GL_COLOR_ATTACHMENT2, 
      GL_COLOR_ATTACHMENT3 };
    glNamedFramebufferDrawBuffers(gfbo, _countof(buffers), buffers);
    if (GLenum status = glCheckNamedFramebufferStatus(gfbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
    {
      throw std::runtime_error("Failed to create framebuffer");
    }

    // setup general vertex format
    GLuint vao;
    glCreateVertexArrays(1, &vao);
    glEnableVertexArrayAttrib(vao, 0);
    glEnableVertexArrayAttrib(vao, 1);
    glEnableVertexArrayAttrib(vao, 2);
    glEnableVertexArrayAttrib(vao, 3);
    glEnableVertexArrayAttrib(vao, 4);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
    glVertexArrayAttribFormat(vao, 3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
    glVertexArrayAttribFormat(vao, 4, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, bitangent));
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribBinding(vao, 1, 0);
    glVertexArrayAttribBinding(vao, 2, 0);
    glVertexArrayAttribBinding(vao, 3, 0);
    glVertexArrayAttribBinding(vao, 4, 0);

    Shader::shaders["gBuffer"].emplace(Shader(
      {
        { "gBuffer.vs", GL_VERTEX_SHADER },
        { "gBuffer.fs", GL_FRAGMENT_SHADER }
      }));
    Shader::shaders["fstexture"].emplace(Shader(
      {
        { "fullscreen_tri.vs", GL_VERTEX_SHADER },
        { "texture.fs", GL_FRAGMENT_SHADER }
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
    Shader::shaders["tonemap"].emplace(Shader(
      {
        { "fullscreen_tri.vs", GL_VERTEX_SHADER },
        { "tonemap.fs", GL_FRAGMENT_SHADER }
      }));
    Shader::shaders["shadow"].emplace(Shader(
      { { "shadow.vs", GL_VERTEX_SHADER } }));

    glClearColor(0, 0, 0, 0);

    // make default camera
    Camera cam;

    // setup lighting
    std::vector<PointLight> localLights;
    float sunPosition = 0;
    DirLight globalLight;
    globalLight.ambient = glm::vec3(.0125f);
    globalLight.diffuse = glm::vec3(.5f);
    globalLight.direction = glm::normalize(glm::vec3(1, -.5, 0));
    globalLight.specular = glm::vec3(.125f);

    int numLights = 5000;
    for (int x = 0; x < numLights; x++)
    {
      PointLight light;
      light.diffuse = glm::vec4(glm::vec3(rng(0, 1), rng(0, 1), rng(0, 1)), 0.f);
      light.specular = light.diffuse;
      light.position = glm::vec4(glm::vec3(rng(-70, 70), rng(.1f, .6f), rng(-30, 30)), 0.f);
      light.linear = rng(2, 8);
      light.quadratic = 0;// rng(5, 12);
      light.radiusSquared = light.CalcRadiusSquared(.010f);
      localLights.push_back(light);
    }
    GLuint lightSSBO{};
    glCreateBuffers(1, &lightSSBO);
    glNamedBufferStorage(lightSSBO, localLights.size() * sizeof(PointLight), localLights.data(), GL_DYNAMIC_STORAGE_BIT);

    // setup geometry
    std::vector<Mesh> terrainMeshes = LoadObj("Resources/Models/sponza/sponza.obj");
    std::vector<Mesh> tmpSphere = LoadObj("Resources/Models/goodSphere.obj");
    Mesh& sphere = tmpSphere[0];

    std::vector<Object> objects;
    Object terrain;
    std::for_each(terrainMeshes.begin(), terrainMeshes.end(), 
      [&terrain](auto& mesh) { terrain.meshes.push_back(&mesh); });
    terrain.scale = glm::vec3(.05f);
    objects.push_back(terrain);

    const int num_objects = 5;
    for (int i = 0; i < num_objects; i++)
    {
      Object a;
      a.meshes = { &sphere };
      a.rotation;
      a.translation = 2.0f * glm::vec3(glm::cos(glm::two_pi<float>() / num_objects * i), 1.0f, glm::sin(glm::two_pi<float>() / num_objects * i));
      a.scale = glm::vec3(1.0f);
      objects.push_back(a);
    }

    Input::SetCursorVisible(false);
    Timer timer;
    float accum = 0;
    while (!glfwWindowShouldClose(window))
    {
      const float dt = timer.elapsed();
      accum += dt;
      if (accum > 1.0f)
      {
        printf("dt: %f (%f FPS)\n", dt, 1.f / dt);
        accum = 0;
      }
      timer.reset();

      Input::Update();
      cam.Update(dt);

      if (Input::IsKeyDown(GLFW_KEY_Q) || Input::IsKeyDown(GLFW_KEY_E))
      {
        if (Input::IsKeyDown(GLFW_KEY_E))
          sunPosition += dt;
        if (Input::IsKeyDown(GLFW_KEY_Q))
          sunPosition -= dt;
        globalLight.direction.x = glm::cos(sunPosition);
        globalLight.direction.y = glm::sin(sunPosition);
        globalLight.direction.z = .1f;
        globalLight.direction = glm::normalize(globalLight.direction);
      }

      objects[1].rotation = glm::rotate(objects[1].rotation, glm::radians(45.f) * dt, glm::vec3(0, 1, 0));

      glBindVertexArray(vao);

      glDisable(GL_BLEND);
      glEnable(GL_DEPTH_TEST);
      glDepthMask(GL_TRUE);
      glEnable(GL_CULL_FACE);
      glFrontFace(GL_CCW);
      glCullFace(GL_BACK);

      const glm::mat4& lightMat = MakeLightMatrix(
        globalLight, -glm::normalize(globalLight.direction) * 200.f + glm::vec3(0, 30, 0), glm::vec2(120), glm::vec2(1.0f, 350.0f));

      // create shadow map pass
      {
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        glClear(GL_DEPTH_BUFFER_BIT);
        //const glm::mat4& lightMat = MakeLightMatrix(
        //  globalLight, -globalLight.direction * 5.f, glm::vec2(10), glm::vec2(1.0f, 10.0f));
        auto& shadowShader = Shader::shaders["shadow"];
        shadowShader->Bind();
        for (const auto& obj : objects)
        {
          shadowShader->SetMat4("u_modelLight", lightMat * obj.GetModelMatrix());
          for (const auto& mesh : obj.meshes)
          {
            glVertexArrayVertexBuffer(vao, 0, mesh->GetID(), 0, sizeof(Vertex));
            glDrawArrays(GL_TRIANGLES, 0, mesh->GetVertexCount());
          }
        }
      }

      glViewport(0, 0, WIDTH, HEIGHT);

      // populate gbuffer pass
      {
        glBindFramebuffer(GL_FRAMEBUFFER, gfbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        auto& gBufferShader = Shader::shaders["gBuffer"];
        gBufferShader->Bind();
        gBufferShader->SetMat4("u_viewProj", cam.GetViewProj());
        gBufferShader->SetInt("u_object.diffuse", 0);
        gBufferShader->SetInt("u_object.alpha", 1);
        gBufferShader->SetInt("u_object.specular", 2);
        gBufferShader->SetInt("u_object.normal", 3);

        for (const auto& obj : objects)
        {
          gBufferShader->SetMat4("u_model", obj.GetModelMatrix());
          gBufferShader->SetMat3("u_normalMatrix", obj.GetNormalMatrix());
          for (const auto& mesh : obj.meshes)
          {
            glVertexArrayVertexBuffer(vao, 0, mesh->GetID(), 0, sizeof(Vertex));
            gBufferShader->SetBool("u_object.hasSpecular", mesh->GetMaterial().hasSpecular);
            gBufferShader->SetBool("u_object.hasAlpha", mesh->GetMaterial().hasAlpha);
            gBufferShader->SetBool("u_object.hasNormal", mesh->GetMaterial().hasNormal);
            gBufferShader->SetFloat("u_object.shininess", mesh->GetMaterial().shininess);
            glBindTextureUnit(0, mesh->GetMaterial().diffuseTex->GetID());
            glBindTextureUnit(1, mesh->GetMaterial().alphaMaskTex->GetID());
            glBindTextureUnit(2, mesh->GetMaterial().specularTex->GetID());
            glBindTextureUnit(3, mesh->GetMaterial().normalTex->GetID());
            glDrawArrays(GL_TRIANGLES, 0, mesh->GetVertexCount());
          }
        }
      }

      glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glBindTextureUnit(0, gPosition);
      glBindTextureUnit(1, gNormal);
      glBindTextureUnit(2, gAlbedoSpec);
      glBindTextureUnit(3, gShininess);
      glBindTextureUnit(4, shadowDepth);

      // global light pass (and apply shadow)
      {
        glDisable(GL_DEPTH_TEST);
        auto& gPhongGlobal = Shader::shaders["gPhongGlobal"];
        gPhongGlobal->Bind();
        gPhongGlobal->SetInt("gPosition", 0);
        gPhongGlobal->SetInt("gNormal", 1);
        gPhongGlobal->SetInt("gAlbedoSpec", 2);
        gPhongGlobal->SetInt("gShininess", 3);
        gPhongGlobal->SetInt("shadowDepth", 4);
        gPhongGlobal->SetVec3("u_viewPos", cam.GetPos());
        gPhongGlobal->SetVec3("u_globalLight.ambient", globalLight.ambient);
        gPhongGlobal->SetVec3("u_globalLight.diffuse", globalLight.diffuse);
        gPhongGlobal->SetVec3("u_globalLight.specular", globalLight.specular);
        gPhongGlobal->SetVec3("u_globalLight.direction", globalLight.direction);
        gPhongGlobal->SetMat4("u_lightMatrix", lightMat);
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }

      // local lights pass
      {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);
        //glFrontFace(GL_CCW);
        auto& gPhongLocal = Shader::shaders["gPhongManyLocal"];
        gPhongLocal->Bind();
        gPhongLocal->SetMat4("u_viewProj", cam.GetViewProj());
        gPhongLocal->SetMat4("u_invProj", glm::inverse(cam.GetProj()));
        gPhongLocal->SetMat4("u_invView", glm::inverse(cam.GetView()));
        gPhongLocal->SetInt("gPosition", 0);
        gPhongLocal->SetInt("gNormal", 1);
        gPhongLocal->SetInt("gAlbedoSpec", 2);
        gPhongLocal->SetInt("gShininess", 3);
        gPhongLocal->SetVec3("u_viewPos", cam.GetPos());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, lightSSBO);
        glVertexArrayVertexBuffer(vao, 0, sphere.GetID(), 0, sizeof(Vertex));
        glDrawArraysInstanced(GL_TRIANGLES, 0, sphere.GetVertexCount(), localLights.size());
      }

      applyTonemapping(dt);

      if (Input::IsKeyDown(GLFW_KEY_1))
      {
        drawFSTexture(gPosition);
      }
      if (Input::IsKeyDown(GLFW_KEY_2))
      {
        drawFSTexture(gAlbedoSpec);
      }
      if (Input::IsKeyDown(GLFW_KEY_3))
      {
        drawFSTexture(gNormal);
      }
      if (Input::IsKeyDown(GLFW_KEY_4))
      {
        drawFSTexture(gDepth);
      }
      if (Input::IsKeyDown(GLFW_KEY_5))
      {
        drawFSTexture(gShininess);
      }
      if (Input::IsKeyDown(GLFW_KEY_6))
      {
        drawFSTexture(shadowDepth);
      }

      if (Input::IsKeyDown(GLFW_KEY_ESCAPE))
      {
        glfwSetWindowShouldClose(window, true);
      }
      glfwSwapBuffers(window);
    }
  }

  void drawFSTexture(GLuint texID)
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

  void applyTonemapping(float dt)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);

    glBindTextureUnit(1, hdrColor);

    const float logLowLum = glm::log(targetLuminance / maxExposure);
    const float logMaxLum = glm::log(targetLuminance / minExposure);

    {
      auto& hshdr = Shader::shaders["generate_histogram"];
      hshdr->Bind();
      hshdr->SetInt("u_hdrBuffer", 1);
      hshdr->SetFloat("u_logLowLum", logLowLum);
      hshdr->SetFloat("u_logMaxLum", logMaxLum);
      const int X_SIZE = 32;
      const int Y_SIZE = 32;
      int xgroups = (WIDTH + X_SIZE - 1) / X_SIZE;
      int ygroups = (HEIGHT + Y_SIZE - 1) / Y_SIZE;
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, histogramBuffer);
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, histogramBuffer);
      glDispatchCompute(xgroups, ygroups, 1);
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    //float expo{};
    //glGetNamedBufferSubData(exposureBuffer, 0, sizeof(float), &expo);
    //printf("Exposure: %f\n", expo);

    {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, exposureBuffer);
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, exposureBuffer);
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, histogramBuffer);
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, histogramBuffer);
      auto& cshdr = Shader::shaders["calc_exposure"];
      cshdr->Bind();
      cshdr->SetFloat("u_dt", dt);
      cshdr->SetFloat("u_adjustmentSpeed", adjustmentSpeed);
      cshdr->SetInt("u_hdrBuffer", 1);
      cshdr->SetFloat("u_logLowLum", logLowLum);
      cshdr->SetFloat("u_logMaxLum", logMaxLum);
      cshdr->SetFloat("u_targetLuminance", targetLuminance);
      glDispatchCompute(1, 1, 1);
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, WIDTH, HEIGHT);
    auto& shdr = Shader::shaders["tonemap"];
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    shdr->Bind();
    shdr->SetFloat("u_exposureFactor", exposureFactor);
    shdr->SetInt("u_hdrBuffer", 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    glDisable(GL_FRAMEBUFFER_SRGB);
  }

  void cleanup()
  {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  // common
  GLFWwindow* window{};
  const uint32_t WIDTH = 1440;
  const uint32_t HEIGHT = 810;

  // deferred stuff
  GLuint gfbo{}, gPosition{}, gAlbedoSpec{}, gNormal{}, gDepth{}, gShininess{};
  GLuint shadowFbo{}, shadowDepth{};
  GLuint SHADOW_WIDTH = 2048;
  GLuint SHADOW_HEIGHT = 2048;

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

int main()
{
  Application app;

  try
  {
    app.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}