#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "Renderer.h"
#include "Shader.h"
#include "Input.h"
#include "Utilities.h"
#include "RendererHelpers.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>

void Renderer::Run()
{
  InitWindow();
  InitGL();
  InitImGui();
  MainLoop();
  Cleanup();
}

void Renderer::InitWindow()
{
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  //glfwWindowHint(GLFW_SAMPLES, NUM_MULTISAMPLES);
  glfwSwapInterval(1);

  window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  Input::Init(window);
  Input::SetCursorVisible(cursorVisible);
}

void Renderer::InitGL()
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
  glEnable(GL_MULTISAMPLE); // for shadows
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &deviceAnisotropy);
}

void Renderer::InitImGui()
{
  const char* glsl_version = "#version 460";
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, false);
  ImGui_ImplOpenGL3_Init(glsl_version);
}

void Renderer::MainLoop()
{
  bluenoiseTex = std::make_unique<Texture2D>("Resources/Textures/bluenoise_32.png", false, false);

  CreateFramebuffers();

  CreateVAO();

  CompileShaders();

  glClearColor(0, 0, 0, 0);

  CreateScene();

  cam.SetPos({ 59.4801331f, 5.45370150f, -6.37605810f });
  cam.SetPitch(-2.98514581f);
  cam.SetYaw(175.706055f);
  cam.Update(0);

  Input::SetCursorVisible(cursorVisible);

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
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (Input::IsKeyPressed(GLFW_KEY_GRAVE_ACCENT))
    {
      cursorVisible = !cursorVisible;
      Input::SetCursorVisible(cursorVisible);
    }

    if (!cursorVisible)
    {
      cam.Update(dt);
    }

    if (Input::IsKeyDown(GLFW_KEY_Q) || Input::IsKeyDown(GLFW_KEY_E))
    {
      if (Input::IsKeyDown(GLFW_KEY_E))
        sunPosition += dt;
      if (Input::IsKeyDown(GLFW_KEY_Q))
        sunPosition -= dt;
      globalLight.direction.x = .1f;
      globalLight.direction.y = glm::sin(sunPosition);
      globalLight.direction.z = glm::cos(sunPosition);
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
    glDepthFunc(GL_LEQUAL);

    const glm::vec3 sunPos = -glm::normalize(globalLight.direction) * 200.f + glm::vec3(0, 30, 0);
    const glm::mat4& lightMat = MakeLightMatrix(
      globalLight, sunPos, glm::vec2(120), glm::vec2(1.0f, 350.0f));

    // create shadow map pass
    {
      glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
      glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
      auto& shadowShader = Shader::shaders["shadow"];
      shadowShader->Bind();
      for (const auto& obj : objects)
      {
        shadowShader->SetMat4("u_modelLight", lightMat * obj.GetModelMatrix());
        for (const auto& mesh : obj.meshes)
        {
          glVertexArrayVertexBuffer(vao, 0, mesh->GetVBOID(), 0, sizeof(Vertex));
          glVertexArrayElementBuffer(vao, mesh->GetEBOID());
          glDrawElements(GL_TRIANGLES, mesh->GetVertexCount(), GL_UNSIGNED_INT, nullptr);
        }
      }
    }

    GLuint filteredTex{};
    switch (shadow_method)
    {
    case SHADOW_METHOD_VSM: filteredTex = vshadowDepthGoodFormat; break;
    case SHADOW_METHOD_ESM: filteredTex = eExpShadowDepth; break;
    case SHADOW_METHOD_MSM: /* TODO: fill */break;
    }

    // copy transformed shadow depth to another texture
    if (shadow_method == SHADOW_METHOD_ESM || shadow_method == SHADOW_METHOD_VSM)
    {
      glBindTextureUnit(0, shadowDepth);

      const char* shaderName = "none";
      GLuint fbo{};
      switch (shadow_method)
      {
      case SHADOW_METHOD_VSM: shaderName = "vsm_copy"; fbo = vshadowGoodFormatFbo; break;
      case SHADOW_METHOD_ESM: shaderName = "esm_copy"; fbo = eShadowFbo; break;
      case SHADOW_METHOD_MSM: shaderName = "msm_copy"; /* TODO: fill */break;
      }

      auto& copyShader = Shader::shaders[shaderName];
      copyShader->Bind();
      if (shadow_method == SHADOW_METHOD_ESM)
      {
        copyShader->SetFloat("u_C", eConstant);
      }
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      if (shadow_method == SHADOW_METHOD_VSM)
      {
        blurTexture32rgf(vshadowDepthGoodFormat, vshadowMomentBlur, SHADOW_WIDTH, SHADOW_HEIGHT, BLUR_PASSES, BLUR_STRENGTH);
      }
      else if (shadow_method == SHADOW_METHOD_ESM)
      {
        blurTexture32rf(eExpShadowDepth, eShadowDepthBlur, SHADOW_WIDTH, SHADOW_HEIGHT, BLUR_PASSES, BLUR_STRENGTH);
      }
      else if (shadow_method == SHADOW_METHOD_MSM)
      {
        // do some blur thing
      }

      if (shadow_gen_mips)
      {
        glGenerateTextureMipmap(filteredTex);
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
      //gBufferShader->SetInt("u_object.normal", 3);

      for (const auto& obj : objects)
      {
        gBufferShader->SetMat4("u_model", obj.GetModelMatrix());
        gBufferShader->SetMat3("u_normalMatrix", obj.GetNormalMatrix());
        for (const auto& mesh : obj.meshes)
        {
          const auto& mat = mesh->GetMaterial();
          gBufferShader->SetBool("u_object.hasSpecular", mat.hasSpecular);
          gBufferShader->SetBool("u_object.hasAlpha", mat.hasAlpha);
          //gBufferShader->SetBool("u_object.hasNormal", mesh->GetMaterial().hasNormal);
          gBufferShader->SetFloat("u_object.shininess", mat.shininess);
          glBindTextureUnit(0, mat.diffuseTex->GetID());
          if (mat.hasAlpha)
          {
            glBindTextureUnit(1, mat.alphaMaskTex->GetID());
          }
          if (mat.hasSpecular)
          {
            glBindTextureUnit(2, mat.specularTex->GetID());
          }
          //glBindTextureUnit(3, mat.normalTex->GetID());
          glVertexArrayVertexBuffer(vao, 0, mesh->GetVBOID(), 0, sizeof(Vertex));
          glVertexArrayElementBuffer(vao, mesh->GetEBOID());
          glDrawElements(GL_TRIANGLES, mesh->GetVertexCount(), GL_UNSIGNED_INT, nullptr);
        }
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindTextureUnit(0, gNormal);
    glBindTextureUnit(1, gAlbedoSpec);
    glBindTextureUnit(2, gShininess);
    glBindTextureUnit(3, gDepth);
    glBindTextureUnit(4, shadowDepth);
    glBindTextureUnit(5, filteredTex);

    // global light pass (and apply shadow)
    {
      glDisable(GL_DEPTH_TEST);
      auto& gPhongGlobal = Shader::shaders["gPhongGlobal"];
      gPhongGlobal->Bind();
      gPhongGlobal->SetInt("u_shadowMethod", shadow_method);
      gPhongGlobal->SetFloat("u_C", eConstant);
      gPhongGlobal->SetVec3("u_viewPos", cam.GetPos());
      gPhongGlobal->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      gPhongGlobal->SetVec3("u_globalLight_ambient", globalLight.ambient);
      gPhongGlobal->SetVec3("u_globalLight_diffuse", globalLight.diffuse);
      gPhongGlobal->SetVec3("u_globalLight_specular", globalLight.specular);
      gPhongGlobal->SetVec3("u_globalLight_direction", globalLight.direction);
      gPhongGlobal->SetMat4("u_lightMatrix", lightMat);
      gPhongGlobal->SetFloat("u_lightBleedFix", vlightBleedFix);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // local lights pass
    {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);
      //glDepthMask(GL_FALSE);
      auto& gPhongLocal = Shader::shaders["gPhongManyLocal"];
      gPhongLocal->Bind();
      gPhongLocal->SetMat4("u_viewProj", cam.GetViewProj());
      gPhongLocal->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      gPhongLocal->SetVec3("u_viewPos", cam.GetPos());
      gPhongLocal->SetInt("gNormal", 0);
      gPhongLocal->SetInt("gAlbedoSpec", 1);
      gPhongLocal->SetInt("gShininess", 2);
      gPhongLocal->SetInt("gDepth", 3);
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, lightSSBO);
      glVertexArrayVertexBuffer(vao, 0, sphere.GetVBOID(), 0, sizeof(Vertex));
      glVertexArrayElementBuffer(vao, sphere.GetEBOID());
      glDrawElementsInstanced(GL_TRIANGLES, sphere.GetVertexCount(), GL_UNSIGNED_INT, nullptr, localLights.size());
      glCullFace(GL_BACK);
    }

    // volumetric/crepuscular/god rays pass (write to volumetrics texture)
    if (volumetric_enabled)
    {
      glViewport(0, 0, VOLUMETRIC_WIDTH, VOLUMETRIC_HEIGHT);
      glBindFramebuffer(GL_FRAMEBUFFER, volumetricsFbo);
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable(GL_BLEND);
      glDisable(GL_DEPTH_TEST);
      glDepthFunc(GL_ALWAYS);
      glDepthMask(GL_FALSE);
      glBindTextureUnit(1, gDepth);
      glBindTextureUnit(2, shadowDepth);
      glBindTextureUnit(3, bluenoiseTex->GetID());
      auto& volumetric = Shader::shaders["volumetric"];
      volumetric->Bind();
      volumetric->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      volumetric->SetMat4("u_lightMatrix", lightMat);
      volumetric->SetIVec2("u_screenSize", VOLUMETRIC_WIDTH, VOLUMETRIC_HEIGHT);
      volumetric->SetInt("NUM_STEPS", volumetric_steps);
      volumetric->SetFloat("intensity", volumetric_intensity);
      volumetric->SetFloat("distToFull", volumetric_distToFull);
      volumetric->SetFloat("noiseOffset", volumetric_noiseOffset);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      if (atrousPasses > 0)
      {
        //blurTexture16rf(volumetricsTex, volumetricsTexBlur, VOLUMETRIC_WIDTH, VOLUMETRIC_HEIGHT, VOLUMETRIC_BLUR_PASSES, VOLUMETRIC_BLUR_STRENGTH);
        glViewport(0, 0, WIDTH, HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, atrousFbo);
        auto& atrousFilter = Shader::shaders["atrous_volumetric"];
        atrousFilter->Bind();
        atrousFilter->SetInt("gColor", 0);
        //atrousFilter->SetInt("gDepth", 1);
        //atrousFilter->SetInt("gNormal", 2);
        atrousFilter->SetFloat("c_phi", c_phi);
        //atrousFilter->SetFloat("n_phi", n_phi);
        //atrousFilter->SetFloat("p_phi", p_phi);
        atrousFilter->SetFloat("stepwidth", stepWidth);
        //atrousFilter->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
        atrousFilter->SetIVec2("u_resolution", VOLUMETRIC_WIDTH, VOLUMETRIC_HEIGHT);
        atrousFilter->Set1FloatArray("kernel[0]", atrouskernel);
        atrousFilter->Set2FloatArray("offsets[0]", atrouskerneloffsets);
      
        glNamedFramebufferDrawBuffer(atrousFbo, GL_COLOR_ATTACHMENT0);
        glBindTextureUnit(0, volumetricsTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (atrousPasses == 2) // two passes (won't support more)
        {
          glNamedFramebufferDrawBuffer(atrousFbo, GL_COLOR_ATTACHMENT1);
          glBindTextureUnit(0, atrousTex);
          glDrawArrays(GL_TRIANGLES, 0, 3);
        }
      }
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glDepthMask(GL_FALSE);

    // screen-space reflections
    if (ssr_enabled)
    {
      glViewport(0, 0, SSR_WIDTH, SSR_HEIGHT);
      glBindFramebuffer(GL_FRAMEBUFFER, ssrFbo);
      glClear(GL_COLOR_BUFFER_BIT);
      auto& ssrShader = Shader::shaders["ssr"];
      ssrShader->Bind();
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBindTextureUnit(0, hdrColor);
      glBindTextureUnit(1, gDepth);
      glBindTextureUnit(2, gShininess);
      glBindTextureUnit(3, gNormal);
      glBindTextureUnit(4, bluenoiseTex->GetID());
      ssrShader->SetInt("gColor", 0);
      ssrShader->SetInt("gDepth", 1);
      ssrShader->SetInt("gShininess", 2);
      ssrShader->SetInt("gNormal", 3);
      ssrShader->SetInt("u_blueNoise", 4);
      ssrShader->SetMat4("u_proj", cam.GetProj());
      ssrShader->SetMat4("u_view", cam.GetView());
      ssrShader->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      ssrShader->SetFloat("rayStep", ssr_rayStep);
      ssrShader->SetFloat("minRayStep", ssr_minRayStep);
      ssrShader->SetFloat("thickness", ssr_thickness);
      ssrShader->SetFloat("searchDist", ssr_searchDist);
      ssrShader->SetInt("maxSteps", ssr_maxRaySteps);
      ssrShader->SetInt("binarySearchSteps", ssr_binarySearchSteps);
      ssrShader->SetIVec2("u_viewportSize", SSR_WIDTH, SSR_HEIGHT);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // composite blurred volumetrics and SSR with unprocessed image
    {
      glViewport(0, 0, WIDTH, HEIGHT);
      glBindFramebuffer(GL_FRAMEBUFFER, postprocessFbo);
      glClear(GL_COLOR_BUFFER_BIT);
      glBlendFunc(GL_ONE, GL_ONE);
      auto& fsshader = Shader::shaders["fstexture"];
      fsshader->Bind();
      fsshader->SetInt("u_texture", 0);
      if (volumetric_enabled)
      {
        if (atrousPasses % 2 == 0)
          glBindTextureUnit(0, volumetricsTex); // uncomment for 0 or 2 pass a-trous
        else
          glBindTextureUnit(0, atrousTex); // uncomment for 1 pass a-trous
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
      glBindTextureUnit(0, hdrColor);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      if (ssr_enabled)
      {
        glBindTextureUnit(0, ssrTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
    }

    // tone mapping + gamma correction pass
    ApplyTonemapping(dt);


    if (Input::IsKeyDown(GLFW_KEY_1))
    {
      drawFSTexture(gAlbedoSpec);
    }
    if (Input::IsKeyDown(GLFW_KEY_2))
    {
      drawFSTexture(gNormal);
    }
    if (Input::IsKeyDown(GLFW_KEY_3))
    {
      drawFSTexture(gDepth);
    }
    if (Input::IsKeyDown(GLFW_KEY_4))
    {
      drawFSTexture(gShininess);
    }
    if (Input::IsKeyDown(GLFW_KEY_5))
    {
      drawFSTexture(shadowDepth);
    }
    if (Input::IsKeyDown(GLFW_KEY_6))
    {
      drawFSTexture(volumetricsTex);
    }
    if (Input::IsKeyDown(GLFW_KEY_7))
    {
      drawFSTexture(atrousTex);
    }
    if (Input::IsKeyDown(GLFW_KEY_8))
    {
      drawFSTexture(ssrTex);
    }
    if (Input::IsKeyDown(GLFW_KEY_9))
    {
      drawFSTexture(eExpShadowDepth);
    }
    if (Input::IsKeyDown(GLFW_KEY_0))
    {
      drawFSTexture(vshadowDepthGoodFormat);
    }
    if (Input::IsKeyPressed(GLFW_KEY_C))
    {
      Shader::shaders.erase("gPhongGlobal");
      Shader::shaders["gPhongGlobal"].emplace(Shader(
        {
          { "fullscreen_tri.vs", GL_VERTEX_SHADER },
          { "gPhongGlobal.fs", GL_FRAGMENT_SHADER }
        }));
      Shader::shaders.erase("volumetric");
      Shader::shaders["volumetric"].emplace(Shader(
        {
          { "fullscreen_tri.vs", GL_VERTEX_SHADER },
          { "volumetric.fs", GL_FRAGMENT_SHADER }
        }));
      Shader::shaders.erase("ssr");
      Shader::shaders["ssr"].emplace(Shader(
        {
          { "fullscreen_tri.vs", GL_VERTEX_SHADER },
          { "ssr.fs", GL_FRAGMENT_SHADER }
        }));
      Shader::shaders.erase("gBuffer");
      Shader::shaders["gBuffer"].emplace(Shader(
        {
          { "gBuffer.vs", GL_VERTEX_SHADER },
          { "gBuffer.fs", GL_FRAGMENT_SHADER }
        }));
      Shader::shaders.erase("fstexture");
      Shader::shaders["fstexture"].emplace(Shader(
        {
          { "fullscreen_tri.vs", GL_VERTEX_SHADER },
          { "texture.fs", GL_FRAGMENT_SHADER }
        }));
    }

    if (Input::IsKeyDown(GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    DrawUI();

    if (cursorVisible)
    {
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(window);
  }
}

void Renderer::CreateFramebuffers()
{
  // create HDR framebuffer
  glCreateTextures(GL_TEXTURE_2D, 1, &hdrColor);
  glTextureStorage2D(hdrColor, 1, GL_RGBA16F, WIDTH, HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &hdrDepth);
  glTextureStorage2D(hdrDepth, 1, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT);
  glCreateFramebuffers(1, &hdrfbo);
  glNamedFramebufferTexture(hdrfbo, GL_COLOR_ATTACHMENT0, hdrColor, 0);
  glNamedFramebufferTexture(hdrfbo, GL_DEPTH_ATTACHMENT, hdrDepth, 0);
  glNamedFramebufferDrawBuffer(hdrfbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(hdrfbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create HDR framebuffer");
  }

  // create SSR framebuffer + textures
  glCreateTextures(GL_TEXTURE_2D, 1, &ssrTex);
  glTextureStorage2D(ssrTex, 1, GL_RGBA16F, SSR_WIDTH, SSR_HEIGHT);
  glTextureParameteri(ssrTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(ssrTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glCreateTextures(GL_TEXTURE_2D, 1, &ssrTexBlur);
  glTextureStorage2D(ssrTexBlur, 1, GL_RGBA16F, SSR_WIDTH, SSR_HEIGHT);
  glCreateFramebuffers(1, &ssrFbo);
  glNamedFramebufferTexture(ssrFbo, GL_COLOR_ATTACHMENT0, ssrTex, 0);
  glNamedFramebufferDrawBuffer(ssrFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(ssrFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create SSR framebuffer");
  }

  // create volumetrics texture + fbo + intermediate (for blurring)
  glCreateTextures(GL_TEXTURE_2D, 1, &volumetricsTex);
  glTextureStorage2D(volumetricsTex, 1, GL_R16F, VOLUMETRIC_WIDTH, VOLUMETRIC_HEIGHT);
  GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
  glTextureParameteriv(volumetricsTex, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  glTextureParameteri(volumetricsTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(volumetricsTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(volumetricsTex, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTextureParameteri(volumetricsTex, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glCreateFramebuffers(1, &volumetricsFbo);
  glNamedFramebufferTexture(volumetricsFbo, GL_COLOR_ATTACHMENT0, volumetricsTex, 0);
  glNamedFramebufferDrawBuffer(volumetricsFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(volumetricsFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create volumetrics framebuffer");
  }
  glCreateTextures(GL_TEXTURE_2D, 1, &volumetricsTexBlur);
  glTextureStorage2D(volumetricsTexBlur, 1, GL_R16F, VOLUMETRIC_WIDTH, VOLUMETRIC_HEIGHT);
  glTextureParameteriv(volumetricsTexBlur, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

  // create a-trous fbo and texture
  glCreateTextures(GL_TEXTURE_2D, 1, &atrousTex);
  glTextureStorage2D(atrousTex, 1, GL_R16F, WIDTH, HEIGHT);
  glTextureParameteriv(atrousTex, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  glTextureParameteri(atrousTex, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTextureParameteri(atrousTex, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glCreateFramebuffers(1, &atrousFbo);
  glNamedFramebufferTexture(atrousFbo, GL_COLOR_ATTACHMENT0, atrousTex, 0);
  glNamedFramebufferTexture(atrousFbo, GL_COLOR_ATTACHMENT1, volumetricsTex, 0);
  glNamedFramebufferDrawBuffer(atrousFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(atrousFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create a-trous framebuffer");
  }

  // create tone mapping buffers
  std::vector<int> zeros(NUM_BUCKETS, 0);
  float expo[] = { exposureFactor, 0 };
  glCreateBuffers(1, &exposureBuffer);
  glNamedBufferStorage(exposureBuffer, 2 * sizeof(float), expo, 0);
  glCreateBuffers(1, &histogramBuffer);
  glNamedBufferStorage(histogramBuffer, NUM_BUCKETS * sizeof(int), zeros.data(), 0);

  const GLfloat txzeros[] = { 0, 0, 0, 0 };
  glCreateTextures(GL_TEXTURE_2D, 1, &shadowDepth);
  glTextureStorage2D(shadowDepth, 1, GL_DEPTH_COMPONENT32, SHADOW_WIDTH, SHADOW_HEIGHT);
  glCreateFramebuffers(1, &shadowFbo);
  //glNamedFramebufferTexture(shadowFbo, GL_COLOR_ATTACHMENT0, shadowDepthSquared, 0);
  glNamedFramebufferTexture(shadowFbo, GL_DEPTH_ATTACHMENT, shadowDepth, 0);
  glNamedFramebufferDrawBuffer(shadowFbo, GL_NONE);
  if (GLenum status = glCheckNamedFramebufferStatus(shadowFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create shadow framebuffer");
  }

  // VSM textures + fbo
  glCreateTextures(GL_TEXTURE_2D, 1, &vshadowDepthGoodFormat);
  glCreateTextures(GL_TEXTURE_2D, 1, &vshadowMomentBlur);
  glTextureStorage2D(vshadowDepthGoodFormat, SHADOW_LEVELS, GL_RG32F, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureStorage2D(vshadowMomentBlur, 1, GL_RG32F, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureParameterfv(vshadowDepthGoodFormat, GL_TEXTURE_BORDER_COLOR, txzeros);
  glTextureParameteri(vshadowDepthGoodFormat, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTextureParameteri(vshadowDepthGoodFormat, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTextureParameteri(vshadowDepthGoodFormat, GL_TEXTURE_MIN_FILTER, shadow_gen_mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTextureParameteri(vshadowDepthGoodFormat, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameterf(vshadowDepthGoodFormat, GL_TEXTURE_MAX_ANISOTROPY, deviceAnisotropy);
  glCreateFramebuffers(1, &vshadowGoodFormatFbo);
  glNamedFramebufferTexture(vshadowGoodFormatFbo, GL_COLOR_ATTACHMENT0, vshadowDepthGoodFormat, 0);
  glNamedFramebufferDrawBuffer(vshadowGoodFormatFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(vshadowGoodFormatFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create VSM framebuffer");
  }

  // ESM textures + fbo
  glCreateTextures(GL_TEXTURE_2D, 1, &eExpShadowDepth);
  glCreateTextures(GL_TEXTURE_2D, 1, &eShadowDepthBlur);
  glTextureStorage2D(eExpShadowDepth, SHADOW_LEVELS, GL_R32F, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureStorage2D(eShadowDepthBlur, 1, GL_R32F, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureParameterfv(eExpShadowDepth, GL_TEXTURE_BORDER_COLOR, txzeros);
  glTextureParameteri(eExpShadowDepth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTextureParameteri(eExpShadowDepth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTextureParameteri(eExpShadowDepth, GL_TEXTURE_MIN_FILTER, shadow_gen_mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTextureParameteri(eExpShadowDepth, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameterf(eExpShadowDepth, GL_TEXTURE_MAX_ANISOTROPY, deviceAnisotropy);
  glCreateFramebuffers(1, &eShadowFbo);
  glNamedFramebufferTexture(eShadowFbo, GL_COLOR_ATTACHMENT0, eExpShadowDepth, 0);
  glNamedFramebufferDrawBuffer(eShadowFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(eShadowFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create ESM framebuffer");
  }

  // create texture attachments for gBuffer FBO
  glCreateTextures(GL_TEXTURE_2D, 1, &gAlbedoSpec);
  glTextureStorage2D(gAlbedoSpec, 1, GL_RGBA8, WIDTH, HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &gNormal);
  glTextureStorage2D(gNormal, 1, GL_RG8_SNORM, WIDTH, HEIGHT);
  //glTextureStorage2D(gNormal, 1, GL_RGBA8_SNORM, WIDTH, HEIGHT); // debugging format
  glCreateTextures(GL_TEXTURE_2D, 1, &gShininess);
  glTextureStorage2D(gShininess, 1, GL_R16F, WIDTH, HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &gDepth);
  glTextureStorage2D(gDepth, 1, GL_DEPTH_COMPONENT32, WIDTH, HEIGHT);

  // create gBuffer FBO
  glCreateFramebuffers(1, &gfbo);
  glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT0, gAlbedoSpec, 0);
  glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT1, gNormal, 0);
  glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT2, gShininess, 0);
  glNamedFramebufferTexture(gfbo, GL_DEPTH_ATTACHMENT, gDepth, 0);
  GLenum buffers[] = { GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2 };
  glNamedFramebufferDrawBuffers(gfbo, _countof(buffers), buffers);
  if (GLenum status = glCheckNamedFramebufferStatus(gfbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create gbuffer framebuffer");
  }

  // create postprocess (HDR) framebuffer
  glCreateTextures(GL_TEXTURE_2D, 1, &postprocessColor);
  glTextureStorage2D(postprocessColor, 1, GL_RGBA16F, WIDTH, HEIGHT);
  glCreateFramebuffers(1, &postprocessFbo);
  glNamedFramebufferTexture(postprocessFbo, GL_COLOR_ATTACHMENT0, postprocessColor, 0);
  glNamedFramebufferTexture(postprocessFbo, GL_DEPTH_ATTACHMENT, gDepth, 0);
  glNamedFramebufferDrawBuffer(postprocessFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(postprocessFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create postprocess framebuffer");
  }
}

void Renderer::CreateVAO()
{
  // setup general vertex format
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
  glVertexArrayAttribBinding(vao, 0, 0);
  glVertexArrayAttribBinding(vao, 1, 0);
  glVertexArrayAttribBinding(vao, 2, 0);
  glVertexArrayAttribBinding(vao, 3, 0);
  glVertexArrayAttribBinding(vao, 4, 0);
}

void Renderer::CreateScene()
{
  // setup lighting
  globalLight.ambient = glm::vec3(.0225f);
  globalLight.diffuse = glm::vec3(.5f);
  globalLight.direction = glm::normalize(glm::vec3(1, -.5, 0));
  globalLight.specular = glm::vec3(.125f);

  CreateLocalLights();

  // setup geometry
  terrainMeshes = LoadObj("Resources/Models/sponza/sponza.obj");
  sphere = std::move(LoadObj("Resources/Models/goodSphere.obj")[0]);

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
}

void Renderer::Cleanup()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
}

void Renderer::DrawUI()
{
  {
    ImGui::Begin("Volumetrics");
    ImGui::Checkbox("Enabled", &volumetric_enabled);
    ImGui::Text("Rays");
    ImGui::Separator();
    ImGui::SliderInt("Steps", &volumetric_steps, 1, 100);
    ImGui::SliderFloat("Intensity", &volumetric_intensity, 0.0f, 1.0f, "%.3f", 3.0f);
    ImGui::SliderFloat("Offset", &volumetric_noiseOffset, 0.0f, 1.0f);
    
    ImGui::Text((const char*)(u8"À-Trous"));
    ImGui::Separator();
    ImGui::Text("Passes");
    int passes = atrousPasses;
    ImGui::RadioButton("Zero", &passes, 0);
    ImGui::SameLine(); ImGui::RadioButton("One", &passes, 1);
    ImGui::SameLine(); ImGui::RadioButton("Two", &passes, 2);
    atrousPasses = passes;
    ImGui::SliderFloat("c_phi", &c_phi, .0001f, 10.0f, "%.4f", 4.0f);
    //ImGui::SliderFloat("n_phi", &n_phi, .001f, 10.0f, "%.3f", 2.0f);
    //ImGui::SliderFloat("p_phi", &p_phi, .001f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Step width", &stepWidth, 0.5f, 2.0f, "%.3f");
    ImGui::End();
  }

  {
    ImGui::Begin("Shadows");
    ImGui::Text("Method");
    ImGui::RadioButton("PCF", &shadow_method, SHADOW_METHOD_PCF);
    ImGui::SameLine();
    ImGui::RadioButton("VSM", &shadow_method, SHADOW_METHOD_VSM);
    ImGui::SameLine();
    ImGui::RadioButton("ESM", &shadow_method, SHADOW_METHOD_ESM);
    ImGui::SameLine();
    ImGui::RadioButton("MSM", &shadow_method, SHADOW_METHOD_MSM);
    ImGui::Separator();

    ImGui::SliderInt("Blur passes", &BLUR_PASSES, 0, 5);
    ImGui::SliderInt("Blur width", &BLUR_STRENGTH, 0, 6);
    ImGui::SliderFloat("(VSM) Hardness", &vlightBleedFix, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("(ESM) C", &eConstant, 60, 90);
    if (ImGui::Checkbox("Generate Mips", &shadow_gen_mips))
    {
      if (shadow_gen_mips)
      {
        glTextureParameteri(vshadowDepthGoodFormat, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(eExpShadowDepth, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      }
      else
      {
        glTextureParameteri(vshadowDepthGoodFormat, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(eExpShadowDepth, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      }
    }
    ImGui::End();
  }

  {
    ImGui::Begin("Environment");
    if (ImGui::SliderAngle("Sun angle", &sunPosition, 180, 360))
    {
      globalLight.direction.x = .1f;
      globalLight.direction.y = glm::sin(sunPosition);
      globalLight.direction.z = glm::cos(sunPosition);
      globalLight.direction = glm::normalize(globalLight.direction);
    }
    ImGui::ColorEdit3("Diffuse", &globalLight.diffuse[0]);
    ImGui::ColorEdit3("Ambient", &globalLight.ambient[0]);
    ImGui::ColorEdit3("Specular", &globalLight.specular[0]);
    ImGui::SliderInt("Local lights", &numLights, 0, 40000);
    if (ImGui::Button("Update local lights"))
    {
      CreateLocalLights();
    }
    ImGui::End();
  }

  {
    ImGui::Begin("Postprocessing");
    ImGui::SliderFloat("Luminance", &targetLuminance, 0.01f, 1.0f);
    ImGui::SliderFloat("Exposure factor", &exposureFactor, 0.01f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Adjustment speed", &adjustmentSpeed, 0.0f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Min exposure", &minExposure, 0.01f, 100.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Max exposure", &maxExposure, 0.01f, 100.0f, "%.3f", 2.0f);
    ImGui::End();
  }

  {
    ImGui::Begin("Screen-Space Reflections");
    ImGui::Checkbox("Enabled", &ssr_enabled);
    ImGui::SliderFloat("Step size", &ssr_rayStep, 0.01f, 1.0f);
    ImGui::SliderFloat("Min step", &ssr_minRayStep, 0.01f, 1.0f);
    ImGui::SliderFloat("Thickness", &ssr_thickness, 0.00f, 1.0f);
    ImGui::SliderFloat("Search distance", &ssr_searchDist, 1.0f, 50.0f);
    ImGui::SliderInt("Max steps", &ssr_maxRaySteps, 0, 100);
    ImGui::SliderInt("Binary search steps", &ssr_binarySearchSteps, 0, 10);
    ImGui::End();
  }
  
  {
    ImGui::Begin("View Buffer");
    ImGui::Image((void*)uiViewBuffer, ImVec2(300, 300), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::RadioButton("gAlbedoSpec", &uiViewBuffer, gAlbedoSpec);
    ImGui::RadioButton("gNormal", &uiViewBuffer, gNormal);
    ImGui::RadioButton("gDepth", &uiViewBuffer, gDepth);
    ImGui::RadioButton("gShininess", &uiViewBuffer, gShininess);
    ImGui::RadioButton("hdrColor", &uiViewBuffer, hdrColor);
    ImGui::RadioButton("hdrDepth", &uiViewBuffer, hdrDepth);
    ImGui::RadioButton("shadowDepthGoodFormat", &uiViewBuffer, vshadowDepthGoodFormat);
    ImGui::RadioButton("atrousTex", &uiViewBuffer, atrousTex);
    ImGui::RadioButton("volumetricsTex", &uiViewBuffer, volumetricsTex);
    ImGui::RadioButton("postprocessColor", &uiViewBuffer, postprocessColor);
    ImGui::RadioButton("ssrTex", &uiViewBuffer, ssrTex);
    ImGui::End();
  }
}

void Renderer::ApplyTonemapping(float dt)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_BLEND);

  glBindTextureUnit(1, postprocessColor);

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
  glDisable(GL_DEPTH_TEST);
  shdr->Bind();
  shdr->SetFloat("u_exposureFactor", exposureFactor);
  shdr->SetInt("u_hdrBuffer", 1);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glDisable(GL_FRAMEBUFFER_SRGB);
}

void Renderer::CreateLocalLights()
{
  if (lightSSBO)
  {
    glDeleteBuffers(1, &lightSSBO);
  }

  localLights.clear();
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
  glCreateBuffers(1, &lightSSBO);
  glNamedBufferStorage(lightSSBO, glm::max(size_t(1), localLights.size() * sizeof(PointLight)), localLights.data(), 0);
}
