#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <chrono>

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
    glCreateTextures(GL_TEXTURE_2D, 1, &gPosition);
    glTextureStorage2D(gPosition, 1, GL_RGBA32F, WIDTH, HEIGHT);
    glTextureParameteri(gPosition, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(gPosition, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glCreateTextures(GL_TEXTURE_2D, 1, &gAlbedoSpec);
    glTextureStorage2D(gAlbedoSpec, 1, GL_RGBA8, WIDTH, HEIGHT);
    glTextureParameteri(gAlbedoSpec, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(gAlbedoSpec, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glCreateTextures(GL_TEXTURE_2D, 1, &gNormal);
    glTextureStorage2D(gNormal, 1, GL_RGBA16F, WIDTH, HEIGHT);
    glTextureParameteri(gNormal, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(gNormal, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glCreateTextures(GL_TEXTURE_2D, 1, &gDepth);
    glTextureStorage2D(gDepth, 1, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT);

    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, gPosition, 0);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT1, gAlbedoSpec, 0);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT2, gNormal, 0);
    glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, gDepth, 0);
    GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glNamedFramebufferDrawBuffers(fbo, 3, buffers);
    
    if (GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
    {
      throw std::runtime_error("Failed to create framebuffer");
    }

    meshes.emplace_back("Resources/Models/sponza/sponza.obj");
    meshes.emplace_back("Resources/Models/goodSphere.obj");

    GLuint vao;
    glCreateVertexArrays(1, &vao);
    glVertexArrayVertexBuffer(vao, 0, meshes[0].GetID(), 0, sizeof(Vertex));
    glEnableVertexArrayAttrib(vao, 0);
    glEnableVertexArrayAttrib(vao, 1);
    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribBinding(vao, 1, 0);
    glVertexArrayAttribBinding(vao, 2, 0);

    Shader::shaders["gBuffer"].emplace(Shader(
      {
        { "Resources/Shaders/gBuffer.vs", GL_VERTEX_SHADER },
        { "Resources/Shaders/gBuffer.fs", GL_FRAGMENT_SHADER }
      }));
    Shader::shaders["fstexture"].emplace(Shader(
      {
        { "Resources/Shaders/fullscreen_tri.vs", GL_VERTEX_SHADER },
        { "Resources/Shaders/texture.fs", GL_FRAGMENT_SHADER }
      }));
    Shader::shaders["gPhongGlobal"].emplace(Shader(
      {
        { "Resources/Shaders/fullscreen_tri.vs", GL_VERTEX_SHADER },
        { "Resources/Shaders/gPhongGlobal.fs", GL_FRAGMENT_SHADER }
      }));
    Shader::shaders["gPhongManyLocale"].emplace(Shader(
      {
        { "Resources/Shaders/lightGeom.vs", GL_VERTEX_SHADER },
        { "Resources/Shaders/gPhongManyLocal.fs", GL_FRAGMENT_SHADER }
      }));

    glClearColor(0, .3, .6, 1);

    Camera cam;
    std::vector<Object> objects;
    std::vector<PointLight> localLights;
    DirLight globalLight;
    globalLight.ambient = glm::vec3(.1f);
    globalLight.diffuse = glm::vec3(.8f);
    globalLight.direction = glm::vec3(.3, -1, 0);
    globalLight.specular = glm::vec3(1);

    int numLights = 20000;
    for (int x = 0; x < numLights; x++)
    {
      PointLight light;
      light.diffuse = glm::vec4(glm::vec3(rng(0, 1), rng(0, 1), rng(0, 1)), 0.f);
      light.specular = light.diffuse;
      light.position = glm::vec4(glm::vec3(rng(-70, 70), rng(.1f, .6f), rng(-30, 30)), 0.f);
      light.linear = rng(4, 10);
      light.quadratic = rng(5, 12);
      light.radiusSquared = light.CalcRadiusSquared(.015f);
      localLights.push_back(light);
    }
    GLuint lightSSBO{};
    glCreateBuffers(1, &lightSSBO);
    glNamedBufferStorage(lightSSBO, localLights.size() * sizeof(PointLight), localLights.data(), GL_DYNAMIC_STORAGE_BIT);

    Object terrain;
    terrain.mesh = &meshes[0];
    terrain.scale = glm::vec3(.05f);
    objects.push_back(terrain);

    const int num_objects = 5;
    for (int i = 0; i < num_objects; i++)
    {
      Object a;
      a.mesh = &meshes[1];
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

      objects[1].rotation = glm::rotate(objects[1].rotation, glm::radians(45.f) * dt, glm::vec3(0, 1, 0));

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glBindVertexArray(vao);

      // populate gbuffer pass
      {
        auto& gBufferShader = Shader::shaders["gBuffer"];
        gBufferShader->Bind();
        gBufferShader->SetMat4("u_viewProj", cam.GetViewProj());

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        for (const auto& obj : objects)
        {
          glVertexArrayVertexBuffer(vao, 0, obj.mesh->GetID(), 0, sizeof(Vertex));
          gBufferShader->SetMat4("u_model", obj.GetModelMatrix());
          gBufferShader->SetMat3("u_normalMatrix", obj.GetNormalMatrix());
          glDrawArrays(GL_TRIANGLES, 0, obj.mesh->GetVertexCount());
        }
      }

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glBindTextureUnit(0, gPosition);
      glBindTextureUnit(1, gNormal);
      glBindTextureUnit(2, gAlbedoSpec);

      // global light pass
      {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        auto& gPhongGlobal = Shader::shaders["gPhongGlobal"];
        gPhongGlobal->Bind();
        gPhongGlobal->SetInt("gPosition", 0);
        gPhongGlobal->SetInt("gNormal", 1);
        gPhongGlobal->SetInt("gAlbedoSpec", 2);
        gPhongGlobal->SetVec3("u_viewPos", cam.GetPos());
        gPhongGlobal->SetVec3("u_globalLight.ambient", globalLight.ambient);
        gPhongGlobal->SetVec3("u_globalLight.diffuse", globalLight.diffuse);
        gPhongGlobal->SetVec3("u_globalLight.specular", globalLight.specular);
        gPhongGlobal->SetVec3("u_globalLight.direction", globalLight.direction);
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }

      // local lights pass
      {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_CULL_FACE);
        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);
        //glFrontFace(GL_CCW);
        auto& gPhongLocal = Shader::shaders["gPhongManyLocale"];
        gPhongLocal->Bind();
        gPhongLocal->SetMat4("u_viewProj", cam.GetViewProj());
        gPhongLocal->SetMat4("u_invProj", glm::inverse(cam.GetProj()));
        gPhongLocal->SetMat4("u_invView", glm::inverse(cam.GetView()));
        gPhongLocal->SetInt("gPosition", 0);
        gPhongLocal->SetInt("gNormal", 1);
        gPhongLocal->SetInt("gAlbedoSpec", 2);
        gPhongLocal->SetVec3("u_viewPos", cam.GetPos());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, lightSSBO);
        glVertexArrayVertexBuffer(vao, 0, meshes[1].GetID(), 0, sizeof(Vertex));
        glDrawArraysInstanced(GL_TRIANGLES, 0, meshes[1].GetVertexCount(), localLights.size());
        //for (const auto& light : localLights)
        //{ 
        //  glm::mat4 model(1);
        //  model = glm::translate(model, glm::vec3(light.position));
        //  float radiusSquared = light.CalcRadiusSquared(0.03f);
        //  model = glm::scale(model, glm::vec3(glm::sqrt(radiusSquared)));
        //  gPhongLocal->SetMat4("u_model", model);
        //  gPhongLocal->SetFloat("u_light.radiusSquared", radiusSquared);
        //  gPhongLocal->SetVec4("u_light.diffuse", light.diffuse);
        //  gPhongLocal->SetVec4("u_light.specular", light.specular);
        //  gPhongLocal->SetVec4("u_light.position", light.position);
        //  gPhongLocal->SetFloat("u_light.linear", light.linear);
        //  gPhongLocal->SetFloat("u_light.quadratic", light.quadratic);
        //  glDrawArrays(GL_TRIANGLES, 0, meshes[1].GetVertexCount());
        //}
      }

      //glBlitNamedFramebuffer(fbo, 0, 0, 0, 0, 0, WIDTH, HEIGHT, WIDTH, HEIGHT, GL_COLOR_BUFFER_BIT, GL_LINEAR);
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

  void cleanup()
  {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  GLFWwindow* window{};
  const uint32_t WIDTH = 1440;
  const uint32_t HEIGHT = 810;
  GLuint fbo{}, gPosition{}, gAlbedoSpec{}, gNormal{}, gDepth{};
  std::vector<Mesh> meshes;
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