#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <filesystem>
#include "Renderer.h"
#include "Shader.h"
#include "Input.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>

namespace fss = std::filesystem;

import Utilities;
import GPU.IndirectDraw;

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
  //glfwWindowHint(GLFW_SAMPLES, FRAMEBUFFER_MULTISAMPLES);
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

  glEnable(GL_MULTISAMPLE); // for shadows

  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &deviceAnisotropy);
}

void Renderer::InitImGui()
{
  const char* glsl_version = "#version 460";
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, false);
  ImGui_ImplOpenGL3_Init(glsl_version);
}

void Renderer::MainLoop()
{
  TextureCreateInfo createInfo
  {
    .path = "Resources/Textures/bluenoise_32.png",
    .sRGB = false,
    .generateMips = false,
    .HDR = false,
    .minFilter = GL_LINEAR,
    .magFilter = GL_LINEAR,
  };
  bluenoiseTex = std::make_unique<Texture2D>(createInfo);
  vertexBuffer = std::make_unique<DynamicBuffer>(sizeof(Vertex) * max_vertices, sizeof(Vertex));
  indexBuffer = std::make_unique<DynamicBuffer>(sizeof(uint32_t) * max_vertices, sizeof(uint32_t));

  CreateFramebuffers();

  CreateVAO();

  CompileShaders();

  InitScene();

  Input::SetCursorVisible(cursorVisible);

  glClearColor(0, 0, 0, 0);

  Timer timer;
  float accum = 0;
  while (!glfwWindowShouldClose(window))
  {
    const float dt = static_cast<float>(timer.elapsed());
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

    for (int i = 1; i < batchedObjects.size(); i++)
    {
      glm::vec3& tr = batchedObjects[i].transform.translation;
      tr = glm::vec3(glm::rotate(glm::mat4(1), glm::radians(-30.0f) * dt, glm::vec3(0, 1, 0)) * glm::vec4(tr, 1.0f));
      batchedObjects[i].transform.rotation = glm::rotate(batchedObjects[i].transform.rotation, glm::radians(45.f) * dt, glm::vec3(0, 1, 0));
    }

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

      std::vector<glm::mat4> uniforms;
      for (const auto& obj : batchedObjects)
      {
        for (const auto& mesh : obj.meshes)
        {
          uniforms.emplace_back(lightMat * obj.transform.GetModelMatrix());
        }
      }
      StaticBuffer uniformBuffer(uniforms.data(), uniforms.size() * sizeof(glm::mat4), 0);
      auto& shadowBindlessShader = Shader::shaders["shadowBindless"];
      shadowBindlessShader->Bind();
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, uniformBuffer.ID());
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, uniformBuffer.ID());
      drawIndirectBuffer->Bind(GL_DRAW_INDIRECT_BUFFER);
      glVertexArrayVertexBuffer(vao, 0, vertexBuffer->GetBufferHandle(), 0, sizeof(Vertex));
      glVertexArrayElementBuffer(vao, indexBuffer->GetBufferHandle());
      glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(uniforms.size()), sizeof(DrawElementsIndirectCommand));
    }

    GLuint filteredTex{};
    switch (shadow_method)
    {
    case SHADOW_METHOD_VSM: filteredTex = vshadowDepthGoodFormat; break;
    case SHADOW_METHOD_ESM: filteredTex = eExpShadowDepth; break;
    case SHADOW_METHOD_MSM: filteredTex = msmShadowMoments; break;
    }

    // copy transformed shadow depth to another texture
    if (shadow_method == SHADOW_METHOD_ESM || shadow_method == SHADOW_METHOD_VSM || shadow_method == SHADOW_METHOD_MSM)
    {
      glBindTextureUnit(0, shadowDepth);

      const char* shaderName = "none";
      GLuint fbo{};
      switch (shadow_method)
      {
      case SHADOW_METHOD_VSM: shaderName = "vsm_copy"; fbo = vshadowGoodFormatFbo; break;
      case SHADOW_METHOD_ESM: shaderName = "esm_copy"; fbo = eShadowFbo; break;
      case SHADOW_METHOD_MSM: shaderName = "msm_copy"; fbo = msmShadowFbo; break;
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
        blurTextureRG32f(vshadowDepthGoodFormat, vshadowMomentBlur, SHADOW_WIDTH, SHADOW_HEIGHT, BLUR_PASSES, BLUR_STRENGTH);
      }
      else if (shadow_method == SHADOW_METHOD_ESM)
      {
        blurTextureR32f(eExpShadowDepth, eShadowDepthBlur, SHADOW_WIDTH, SHADOW_HEIGHT, BLUR_PASSES, BLUR_STRENGTH);
      }
      else if (shadow_method == SHADOW_METHOD_MSM)
      {
        blurTextureRGBA32f(msmShadowMoments, msmShadowMomentsBlur, SHADOW_WIDTH, SHADOW_HEIGHT, BLUR_PASSES, BLUR_STRENGTH);
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

      // draw batched objects
      std::vector<ObjectUniforms> uniforms;
      for (const auto& obj : batchedObjects)
      {
        for (const auto& mesh : obj.meshes)
        {
          ObjectUniforms uniform
          {
            .modelMatrix = obj.transform.GetModelMatrix(),
            .normalMatrix = obj.transform.GetNormalMatrix(),
            .materialIndex = mesh.materialIndex
          };
          uniforms.push_back(uniform);
        }
      }
      StaticBuffer uniformBuffer(uniforms.data(), sizeof(ObjectUniforms) * uniforms.size(), 0);

      auto& gbufBindless = Shader::shaders["gBufferBindless"];
      gbufBindless->Bind();
      gbufBindless->SetMat4("u_viewProj", cam.GetViewProj());
      gbufBindless->SetBool("u_materialOverride", materialOverride);
      gbufBindless->SetVec3("u_albedoOverride", albedoOverride);
      gbufBindless->SetFloat("u_roughnessOverride", roughnessOverride);
      gbufBindless->SetFloat("u_metalnessOverride", metalnessOverride);
      gbufBindless->SetFloat("u_ambientOcclusionOverride", ambientOcclusionOverride);
      uniformBuffer.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
      materialsBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 1);
      drawIndirectBuffer->Bind(GL_DRAW_INDIRECT_BUFFER);
      glVertexArrayVertexBuffer(vao, 0, vertexBuffer->GetBufferHandle(), 0, sizeof(Vertex));
      glVertexArrayElementBuffer(vao, indexBuffer->GetBufferHandle());
      glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, uniforms.size(), sizeof(DrawElementsIndirectCommand));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, hdrfbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindTextureUnit(0, gNormal);
    glBindTextureUnit(1, gAlbedo);
    glBindTextureUnit(2, gRMA);
    glBindTextureUnit(3, gDepth);
    glBindTextureUnit(4, shadowDepth);
    glBindTextureUnit(5, filteredTex);
    glBindTextureUnit(6, envMap_irradiance->GetID());
    glBindTextureUnit(7, envMap_hdri->GetID());

    // global light pass (and apply shadow)
    {
      glDisable(GL_DEPTH_TEST);
      auto& gPhongGlobal = Shader::shaders["gPhongGlobal"];
      gPhongGlobal->Bind();
      gPhongGlobal->SetInt("u_shadowMethod", shadow_method);
      gPhongGlobal->SetFloat("u_C", eConstant);
      gPhongGlobal->SetVec3("u_viewPos", cam.GetPos());
      gPhongGlobal->SetIVec2("u_screenSize", WIDTH, HEIGHT);
      gPhongGlobal->SetUInt("u_samples", numEnvSamples);
      gPhongGlobal->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      //gPhongGlobal->SetVec3("u_globalLight_ambient", globalLight.ambient);
      gPhongGlobal->SetVec3("u_globalLight_diffuse", globalLight.diffuse);
      //gPhongGlobal->SetVec3("u_globalLight_specular", globalLight.specular);
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
      gPhongLocal->SetInt("gAlbedo", 1);
      gPhongLocal->SetInt("gRMA", 2);
      gPhongLocal->SetInt("gDepth", 3);
      lightSSBO->BindBase(GL_SHADER_STORAGE_BUFFER, 0);
      glVertexArrayVertexBuffer(vao, 0, sphere.GetVBOID(), 0, sizeof(Vertex));
      glVertexArrayElementBuffer(vao, sphere.GetEBOID());
      glDrawElementsInstanced(GL_TRIANGLES, sphere.GetVertexCount(), GL_UNSIGNED_INT, nullptr, localLights.size());
      glCullFace(GL_BACK);
    }

    // skybox pass
    {
      glBlitNamedFramebuffer(gfbo, hdrfbo,
        0, 0, WIDTH, HEIGHT,
        0, 0, WIDTH, HEIGHT,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
      glEnable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);
      glDepthFunc(GL_LEQUAL);
      glBlendFunc(GL_ONE, GL_ZERO);

      auto& hdriShader = Shader::shaders["hdri_skybox"];
      hdriShader->Bind();
      hdriShader->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      hdriShader->SetIVec2("u_screenSize", WIDTH, HEIGHT);
      hdriShader->SetVec3("u_camPos", cam.GetPos());
      envMap_hdri->Bind(0);
      //envMap_irradiance->Bind(0);
      glDrawArrays(GL_TRIANGLES, 0, 3);
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
      glBindTextureUnit(2, gRMA);
      glBindTextureUnit(3, gNormal);
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
      glBindTextureUnit(0, hdrColor);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      if (volumetric_enabled)
      {
        if (atrousPasses % 2 == 0)
          glBindTextureUnit(0, volumetricsTex); // 0 or 2 pass a-trous
        else
          glBindTextureUnit(0, atrousTex); // 1 pass a-trous
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
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
      drawFSTexture(gAlbedo);
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
      drawFSTexture(gRMA);
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
      drawFSTexture(msmShadowMoments);
    }
    if (Input::IsKeyDown(GLFW_KEY_0))
    {
      drawFSTexture(vshadowDepthGoodFormat);
    }
    if (Input::IsKeyDown(GLFW_KEY_P))
    {
      drawFSTexture(envMap_hdri->GetID());
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

    if (cursorVisible)
    {
      DrawUI();
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
  glTextureStorage2D(hdrDepth, 1, GL_DEPTH_COMPONENT32, WIDTH, HEIGHT);
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
  exposureBuffer = std::make_unique<StaticBuffer>(expo, 2 * sizeof(float), 0);
  histogramBuffer = std::make_unique<StaticBuffer>(zeros.data(), zeros.size() * sizeof(int), 0);

  const GLfloat txzeros[] = { 0, 0, 0, 0 };
  glCreateTextures(GL_TEXTURE_2D, 1, &shadowDepth);
  glTextureStorage2D(shadowDepth, 1, GL_DEPTH_COMPONENT32, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureParameterfv(shadowDepth, GL_TEXTURE_BORDER_COLOR, txzeros);
  glTextureParameteri(shadowDepth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTextureParameteri(shadowDepth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glCreateFramebuffers(1, &shadowFbo);
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

  // MSM textures + fbo
  glCreateTextures(GL_TEXTURE_2D, 1, &msmShadowMoments);
  glCreateTextures(GL_TEXTURE_2D, 1, &msmShadowMomentsBlur);
  glTextureStorage2D(msmShadowMoments, SHADOW_LEVELS, GL_RGBA32F, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureStorage2D(msmShadowMomentsBlur, 1, GL_RGBA32F, SHADOW_WIDTH, SHADOW_HEIGHT);
  glTextureParameterfv(msmShadowMoments, GL_TEXTURE_BORDER_COLOR, txzeros);
  glTextureParameteri(msmShadowMoments, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTextureParameteri(msmShadowMoments, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTextureParameteri(msmShadowMoments, GL_TEXTURE_MIN_FILTER, shadow_gen_mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTextureParameteri(msmShadowMoments, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameterf(msmShadowMoments, GL_TEXTURE_MAX_ANISOTROPY, deviceAnisotropy);
  glCreateFramebuffers(1, &msmShadowFbo);
  glNamedFramebufferTexture(msmShadowFbo, GL_COLOR_ATTACHMENT0, msmShadowMoments, 0);
  glNamedFramebufferDrawBuffer(msmShadowFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(msmShadowFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create MSM framebuffer");
  }

  // create texture attachments for gBuffer FBO
  glCreateTextures(GL_TEXTURE_2D, 1, &gAlbedo);
  glTextureStorage2D(gAlbedo, 1, GL_RGBA8, WIDTH, HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &gNormal);
  glTextureStorage2D(gNormal, 1, GL_RG8_SNORM, WIDTH, HEIGHT);
  //glTextureStorage2D(gNormal, 1, GL_RGBA8_SNORM, WIDTH, HEIGHT); // debugging format
  glCreateTextures(GL_TEXTURE_2D, 1, &gRMA);
  glTextureStorage2D(gRMA, 1, GL_RGB10_A2, WIDTH, HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &gDepth);
  glTextureStorage2D(gDepth, 1, GL_DEPTH_COMPONENT32, WIDTH, HEIGHT);

  // create gBuffer FBO
  glCreateFramebuffers(1, &gfbo);
  glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT0, gAlbedo, 0);
  glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT1, gNormal, 0);
  glNamedFramebufferTexture(gfbo, GL_COLOR_ATTACHMENT2, gRMA, 0);
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

void Renderer::InitScene()
{
  globalLight.direction = glm::normalize(glm::vec3(1, -.5f, 0));

  sphere = std::move(LoadObjMesh("Resources/Models/goodSphere.obj", materialManager)[0]);

  LoadScene2();
}

void Renderer::Cleanup()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glDeleteVertexArrays(1, &vao);

  // do not gaze at it too closely
  glDeleteTextures(1, &volumetricsTex);
  glDeleteTextures(1, &volumetricsTexBlur);
  glDeleteFramebuffers(1, &volumetricsFbo);

  glDeleteTextures(1, &atrousTex);
  glDeleteFramebuffers(1, &atrousFbo);

  glDeleteTextures(1, &gAlbedo);
  glDeleteTextures(1, &gNormal);
  glDeleteTextures(1, &gDepth);
  glDeleteTextures(1, &gRMA);
  glDeleteFramebuffers(1, &gfbo);

  glDeleteTextures(1, &postprocessColor);
  glDeleteFramebuffers(1, &postprocessFbo);

  glDeleteTextures(1, &ssrTex);
  glDeleteTextures(1, &ssrTexBlur);
  glDeleteFramebuffers(1, &ssrFbo);

  glDeleteTextures(1, &shadowDepth);
  glDeleteFramebuffers(1, &shadowFbo);

  glDeleteTextures(1, &vshadowDepthGoodFormat);
  glDeleteTextures(1, &vshadowMomentBlur);
  glDeleteFramebuffers(1, &vshadowGoodFormatFbo);

  glDeleteTextures(1, &eExpShadowDepth);
  glDeleteTextures(1, &eShadowDepthBlur);
  glDeleteFramebuffers(1, &eShadowFbo);

  glDeleteTextures(1, &msmShadowMoments);
  glDeleteTextures(1, &msmShadowMomentsBlur);
  glDeleteFramebuffers(1, &msmShadowFbo);

  glDeleteTextures(1, &hdrColor);
  glDeleteTextures(1, &hdrDepth);
  glDeleteFramebuffers(1, &hdrfbo);

  glfwDestroyWindow(window);
  glfwTerminate();
}

void Renderer::DrawUI()
{
  ImGui::Begin("Volumetrics");
  {
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
  }
  ImGui::End();

  ImGui::Begin("Shadows");
  {
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
  }
  ImGui::End();

  ImGui::Begin("Environment");
  {
    if (ImGui::SliderAngle("Sun angle", &sunPosition, 180, 360))
    {
      globalLight.direction.x = .1f;
      globalLight.direction.y = glm::sin(sunPosition);
      globalLight.direction.z = glm::cos(sunPosition);
      globalLight.direction = glm::normalize(globalLight.direction);
    }
    ImGui::ColorEdit3("Diffuse", &globalLight.diffuse[0]);

    ImGui::SliderFloat("Light Volume Threshold", &lightVolumeThreshold, 0.001f, 0.1f);
  }
  ImGui::End();

  ImGui::Begin("Postprocessing");
  {
    ImGui::SliderFloat("Luminance", &targetLuminance, 0.01f, 1.0f);
    ImGui::SliderFloat("Exposure factor", &exposureFactor, 0.01f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Adjustment speed", &adjustmentSpeed, 0.0f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Min exposure", &minExposure, 0.01f, 100.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Max exposure", &maxExposure, 0.01f, 100.0f, "%.3f", 2.0f);
  }
  ImGui::End();

  ImGui::Begin("Screen-Space Reflections");
  {
    ImGui::Checkbox("Enabled", &ssr_enabled);
    ImGui::SliderFloat("Step size", &ssr_rayStep, 0.01f, 1.0f);
    ImGui::SliderFloat("Min step", &ssr_minRayStep, 0.01f, 1.0f);
    ImGui::SliderFloat("Thickness", &ssr_thickness, 0.00f, 1.0f);
    ImGui::SliderFloat("Search distance", &ssr_searchDist, 1.0f, 50.0f);
    ImGui::SliderInt("Max steps", &ssr_maxRaySteps, 0, 100);
    ImGui::SliderInt("Binary search steps", &ssr_binarySearchSteps, 0, 10);
  }
  ImGui::End();

  ImGui::Begin("View Buffer");
  {
    ImGui::Image((void*)uiViewBuffer, ImVec2(300, 300), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::RadioButton("gAlbedo", &uiViewBuffer, gAlbedo);
    ImGui::RadioButton("gNormal", &uiViewBuffer, gNormal);
    ImGui::RadioButton("gDepth", &uiViewBuffer, gDepth);
    ImGui::RadioButton("gRMA", &uiViewBuffer, gRMA);
    ImGui::RadioButton("hdrColor", &uiViewBuffer, hdrColor);
    ImGui::RadioButton("hdrDepth", &uiViewBuffer, hdrDepth);
    ImGui::RadioButton("shadowDepthGoodFormat", &uiViewBuffer, vshadowDepthGoodFormat);
    ImGui::RadioButton("atrousTex", &uiViewBuffer, atrousTex);
    ImGui::RadioButton("volumetricsTex", &uiViewBuffer, volumetricsTex);
    ImGui::RadioButton("postprocessColor", &uiViewBuffer, postprocessColor);
    ImGui::RadioButton("ssrTex", &uiViewBuffer, ssrTex);
  }
  ImGui::End();

  ImGui::Begin("Scene");
  {
    ImGui::SliderInt("Local lights", &numLights, 0, 40000);
    if (ImGui::SliderFloat2("Light falloff", glm::value_ptr(lightFalloff), 0.01f, 10.0f, "%.3f", 2.0f))
    {
      if (lightFalloff.y < lightFalloff.x)
      {
        std::swap(lightFalloff.x, lightFalloff.y);
      }
    }

    ImGui::SliderInt("Num Env Samples", &numEnvSamples, 1, 100);
    ImGui::Separator();
    ImGui::Checkbox("Material Override", &materialOverride);
    ImGui::ColorEdit3("Albedo Override", &albedoOverride[0]);
    ImGui::SliderFloat("Roughness Override", &roughnessOverride, 0.0f, 1.0f);
    ImGui::SliderFloat("Metalness Override", &metalnessOverride, 0.0f, 1.0f);

    if (ImGui::Button("Load Scene 1"))
    {
      LoadScene1();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene 2"))
    {
      LoadScene2();
    }

    if (ImGui::Button("Lights: Scene 1"))
    {
      Scene1Lights();
    }
    ImGui::SameLine();
    if (ImGui::Button("Lights: Scene 2"))
    {
      Scene2Lights();
    }
    ImGui::Separator();
    ImGui::Text("Env Map");
    for (auto& p : fss::directory_iterator("Resources/IBL"))
    {
      std::string str = p.path().string();
      if (ImGui::Button(str.c_str()))
      {
        TextureCreateInfo info
        {
          .path = str,
          .sRGB = false,
          .generateMips = true,
          .HDR = true,
          .minFilter = GL_LINEAR_MIPMAP_LINEAR,
          .magFilter = GL_LINEAR,
        };

        if (str.find(".irr") == std::string::npos)
        {
          //info.sRGB = true;
          envMap_hdri = std::make_unique<Texture2D>(info);
        }
        else
        {
          info.generateMips = false;
          info.minFilter = GL_LINEAR;
          envMap_irradiance = std::make_unique<Texture2D>(info);
        }
      }
    }
  }
  ImGui::End();

  ImGui::Begin("Lights");
  {
    for (int i = 0; i < localLights.size(); i++)
    {
      std::string id = "##light" + std::to_string(i);
      PointLight& light = localLights[i];

      bool modified = false;
      if (ImGui::ColorEdit3(("Diffuse" + id).c_str(), &light.diffuse[0]))
        modified = true;
      if (ImGui::SliderFloat3(("Position" + id).c_str(), &light.position[0], -10, 10))
        modified = true;
      if (ImGui::SliderFloat(("Linear" + id).c_str(), &light.linear, 0.01, 10))
        modified = true;
      if (ImGui::SliderFloat(("Quadratic" + id).c_str(), &light.quadratic, 0.01, 10))
        modified = true;

      if (modified)
      {
        light.radiusSquared = light.CalcRadiusSquared(lightVolumeThreshold);
        lightSSBO->SubData(&light, sizeof(PointLight), i * sizeof(PointLight));
      }
    }
  }
  ImGui::End();
}

void Renderer::ApplyTonemapping(float dt)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_BLEND);

  glBindTextureUnit(1, postprocessColor);

  const float logLowLum = glm::log(targetLuminance / maxExposure);
  const float logMaxLum = glm::log(targetLuminance / minExposure);
  const int computePixelsX = WIDTH / 2;
  const int computePixelsY = HEIGHT / 2;

  {
    auto& hshdr = Shader::shaders["generate_histogram"];
    hshdr->Bind();
    hshdr->SetInt("u_hdrBuffer", 1);
    hshdr->SetFloat("u_logLowLum", logLowLum);
    hshdr->SetFloat("u_logMaxLum", logMaxLum);
    const int X_SIZE = 8;
    const int Y_SIZE = 8;
    int xgroups = (computePixelsX + X_SIZE - 1) / X_SIZE;
    int ygroups = (computePixelsY + Y_SIZE - 1) / Y_SIZE;
    histogramBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 0);
    glDispatchCompute(xgroups, ygroups, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }

  //float expo{};
  //glGetNamedBufferSubData(exposureBuffer, 0, sizeof(float), &expo);
  //printf("Exposure: %f\n", expo);

  {
    exposureBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 0);
    histogramBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 1);
    auto& cshdr = Shader::shaders["calc_exposure"];
    cshdr->Bind();
    cshdr->SetFloat("u_dt", dt);
    cshdr->SetFloat("u_adjustmentSpeed", adjustmentSpeed);
    cshdr->SetFloat("u_logLowLum", logLowLum);
    cshdr->SetFloat("u_logMaxLum", logMaxLum);
    cshdr->SetFloat("u_targetLuminance", targetLuminance);
    cshdr->SetInt("u_numPixels", computePixelsX * computePixelsY);
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

void Renderer::Scene1Lights()
{
  globalLight.diffuse = glm::vec3(1.0f);

  localLights.clear();
  for (int x = 0; x < numLights; x++)
  {
    PointLight light;
    light.diffuse = glm::vec4(glm::vec3(rng(0, 1), rng(0, 1), rng(0, 1)), 0.f);
    light.position = glm::vec4(glm::vec3(rng(-70, 70), rng(.1f, .6f), rng(-30, 30)), 0.f);
    light.linear = rng(lightFalloff.x, lightFalloff.y);
    light.quadratic = 0;// rng(5, 12);
    light.radiusSquared = light.CalcRadiusSquared(lightVolumeThreshold);
    localLights.push_back(light);
  }
  lightSSBO = std::make_unique<StaticBuffer>(localLights.data(), glm::max(size_t(1),
    localLights.size() * sizeof(PointLight)), GL_DYNAMIC_STORAGE_BIT);
}

void Renderer::Scene2Lights()
{
  globalLight.diffuse = glm::vec3(0);

  localLights.clear();

  PointLight light;
  light.diffuse = glm::vec4(5.f, 5.f, 5.f, 0.f);
  light.position = glm::vec4(2, 3, 0, 0);
  light.linear = 0;
  light.quadratic = 1;
  light.radiusSquared = light.CalcRadiusSquared(lightVolumeThreshold);
  localLights.push_back(light);

  lightSSBO = std::make_unique<StaticBuffer>(localLights.data(), glm::max(size_t(1),
    localLights.size() * sizeof(PointLight)), GL_DYNAMIC_STORAGE_BIT);
}

void Renderer::LoadScene1()
{
  cam.SetPos({ 59.4801331f, 5.45370150f, -6.37605810f });
  cam.SetPitch(-2.98514581f);
  cam.SetYaw(175.706055f);
  cam.Update(0);

  vertexBuffer->Clear();
  indexBuffer->Clear();
  batchedObjects.clear();
  Scene1Lights();

  TextureCreateInfo createInfo
  {
    .path = "Resources/IBL/Arches_E_PineTree_3k.hdr",
    .sRGB = false,
    .generateMips = true,
    .HDR = true,
    .minFilter = GL_LINEAR_MIPMAP_LINEAR,
    .magFilter = GL_LINEAR,
  };

  envMap_hdri = std::make_unique<Texture2D>(createInfo);
  createInfo.path = "Resources/IBL/Arches_E_PineTree_3k.irr.hdr";
  createInfo.generateMips = false;
  createInfo.minFilter = GL_LINEAR;
  envMap_irradiance = std::make_unique<Texture2D>(createInfo);

  auto sphereBatched = LoadObjBatch("Resources/Models/bunny.obj", materialManager, *vertexBuffer, *indexBuffer)[0];

  auto terrain2 = LoadObjBatch("Resources/Models/sponza/sponza.obj", materialManager, *vertexBuffer, *indexBuffer);

  auto tempMatsStr = materialManager.GetLinearMaterials();
  for (size_t i = 0; i < tempMatsStr.size(); i++)
  {
    if (sphereBatched.materialName == tempMatsStr[i].first)
    {
      sphereBatched.materialIndex = i;
      break;
    }
  }

  ObjectBatched terrain2b;
  for (auto& mesh : terrain2)
  {
    for (size_t i = 0; i < tempMatsStr.size(); i++)
    {
      if (mesh.materialName == tempMatsStr[i].first)
      {
        mesh.materialIndex = i;
        break;
      }
    }
    terrain2b.meshes.push_back(mesh);
  }
  terrain2b.transform.scale = glm::vec3(.05f);
  batchedObjects.push_back(terrain2b);

  const int num_spheres = 5;
  for (int i = 0; i < num_spheres; i++)
  {
    ObjectBatched a;
    a.meshes = { sphereBatched };
    a.transform.rotation;
    a.transform.translation = 2.0f * glm::vec3(glm::cos(glm::two_pi<float>() / num_spheres * i), 1.0f, glm::sin(glm::two_pi<float>() / num_spheres * i));
    a.transform.scale = glm::vec3(1.0f);
    batchedObjects.push_back(a);
  }

  SetupBuffers();
}

void Renderer::LoadScene2()
{
  cam.SetPos({ 4.1f, 2.6f, 2.5f });
  cam.SetPitch(-13.0f);
  cam.SetYaw(209.0f);
  cam.Update(0);

  vertexBuffer->Clear();
  indexBuffer->Clear();
  batchedObjects.clear();
  Scene2Lights();

  TextureCreateInfo createInfo
  {
    .path = "Resources/IBL/Arches_E_PineTree_3k.hdr",
    .sRGB = false,
    .generateMips = true,
    .HDR = true,
    .minFilter = GL_LINEAR_MIPMAP_LINEAR,
    .magFilter = GL_LINEAR,
  };

  envMap_hdri = std::make_unique<Texture2D>(createInfo);
  createInfo.path = "Resources/IBL/Arches_E_PineTree_3k.irr.hdr";
  createInfo.generateMips = false;
  createInfo.minFilter = GL_LINEAR;
  envMap_irradiance = std::make_unique<Texture2D>(createInfo);

  //auto model = LoadObjBatch("Resources/Models/avocado/avocado.obj", materialManager, *vertexBuffer, *indexBuffer);
  auto model = LoadObjBatch("Resources/Models/motorcycle/Srad 750.obj", materialManager, *vertexBuffer, *indexBuffer);

  auto tempMatsStr = materialManager.GetLinearMaterials();

  ObjectBatched modelbatched;
  for (auto& mesh : model)
  {
    for (size_t i = 0; i < tempMatsStr.size(); i++)
    {
      if (mesh.materialName == tempMatsStr[i].first)
      {
        mesh.materialIndex = i;
        break;
      }
    }
    modelbatched.meshes.push_back(mesh);
  }
  modelbatched.transform.scale = glm::vec3(2);
  batchedObjects.push_back(modelbatched);

  SetupBuffers();
}

void Renderer::SetupBuffers()
{
  // setup material buffer
  auto tempMatsStr = materialManager.GetLinearMaterials();
  std::vector<BindlessMaterial> tempMats;
  for (const auto& matp : tempMatsStr)
  {
    BindlessMaterial bm
    {
      .albedoHandle = matp.second.albedoTex->GetBindlessHandle(),
      .roughnessHandle = matp.second.roughnessTex->GetBindlessHandle(),
      .metalnessHandle = matp.second.metalnessTex->GetBindlessHandle(),
      .normalHandle = matp.second.normalTex->GetBindlessHandle(),
      .ambientOcclusionHandle = matp.second.ambientOcclusionTex->GetBindlessHandle(),
    };
    tempMats.push_back(bm);
  }
  materialsBuffer = std::make_unique<StaticBuffer>(tempMats.data(), tempMats.size() * sizeof(BindlessMaterial), 0);

  // setup indirect draw buffer
  std::vector<DrawElementsIndirectCommand> cmds;
  GLuint baseInstance = 0;
  for (const auto& obj : batchedObjects)
  {
    for (const auto& mesh : obj.meshes)
    {
      const auto& vtxInfo = vertexBuffer->GetAlloc(mesh.verticesAllocHandle);
      const auto& idxInfo = indexBuffer->GetAlloc(mesh.indicesAllocHandle);
      DrawElementsIndirectCommand cmd
      {
        .count = idxInfo.size / sizeof(uint32_t),
        .instanceCount = 1,
        .firstIndex = idxInfo.offset / sizeof(uint32_t),
        .baseVertex = vtxInfo.offset / sizeof(Vertex),
        .baseInstance = baseInstance++,
      };
      cmds.push_back(cmd);
    }
  }
  drawIndirectBuffer = std::make_unique<StaticBuffer>(cmds.data(), sizeof(DrawElementsIndirectCommand) * cmds.size(), 0);
}