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
  glfwSwapInterval(vsyncEnabled);

  window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "GLest Rendererer", nullptr, nullptr);
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
  while (!glfwWindowShouldClose(window))
  {
    const float dt = static_cast<float>(timer.elapsed());
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

    if (Input::IsKeyDown(GLFW_KEY_LEFT_BRACKET))
    {
      magnifierScale = glm::max(0.002f, magnifierScale * (1.0f - 1.5f * dt));
    }
    if (Input::IsKeyDown(GLFW_KEY_RIGHT_BRACKET))
    {
      magnifierScale = glm::min(0.1f, magnifierScale * (1.0f + 1.5f * dt));
    }
    if (Input::IsKeyPressed(GLFW_KEY_SPACE))
    {
      magnifierLock = !magnifierLock;
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

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

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
            //.normalMatrix = obj.transform.GetNormalMatrix(),
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
      gbufBindless->SetFloat("u_AOoverride", AOoverride);
      gbufBindless->SetFloat("u_ambientOcclusionOverride", ambientOcclusionOverride);
      uniformBuffer.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
      materialsBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 1);
      drawIndirectBuffer->Bind(GL_DRAW_INDIRECT_BUFFER);
      glVertexArrayVertexBuffer(vao, 0, vertexBuffer->GetBufferHandle(), 0, sizeof(Vertex));
      glVertexArrayElementBuffer(vao, indexBuffer->GetBufferHandle());
      glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, uniforms.size(), sizeof(DrawElementsIndirectCommand));
      
      if (drawPbrSphereGridQuestionMark)
      {
        DrawPbrSphereGrid();
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo);
    //glClear(GL_COLOR_BUFFER_BIT);
    float unovec[] = { 1, 1, 1, 1 };
    glClearNamedFramebufferfv(ssao.fbo, GL_COLOR, 0, unovec);
    glBindTextureUnit(0, gDepth);
    glBindTextureUnit(1, gNormal);

    // SSAO pass
    if (ssao.enabled)
    {
      auto& ssaoShader = Shader::shaders["ssao"];
      ssaoShader->Bind();
      ssaoShader->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      ssaoShader->SetMat4("u_view", cam.GetView());
      ssaoShader->SetUInt("u_numSamples", ssao.samples_near);
      ssaoShader->SetFloat("u_delta", ssao.delta);
      ssaoShader->SetFloat("u_R", ssao.range);
      ssaoShader->SetFloat("u_s", ssao.s);
      ssaoShader->SetFloat("u_k", ssao.k);
      glNamedFramebufferTexture(ssao.fbo, GL_COLOR_ATTACHMENT0, ssao.texture, 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      if (ssao.atrous_passes > 0)
      {
        glBindTextureUnit(1, gDepth);
        glBindTextureUnit(2, gNormal);
        auto& ssaoblur = Shader::shaders["atrous_ssao"];
        ssaoblur->Bind();
        ssaoblur->SetFloat("n_phi", ssao.atrous_n_phi);
        ssaoblur->SetFloat("p_phi", ssao.atrous_p_phi);
        ssaoblur->SetFloat("stepwidth", ssao.atrous_step_width);
        ssaoblur->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
        ssaoblur->SetIVec2("u_resolution", WINDOW_WIDTH, WINDOW_HEIGHT);
        ssaoblur->Set1FloatArray("kernel[0]", ssao.atrous_kernel);
        ssaoblur->Set1FloatArray("offsets[0]", ssao.atrous_offsets);
        for (int i = 0; i < ssao.atrous_passes; i++)
        {
          float offsets2[5];
          for (int j = 0; j < 5; j++)
          {
            offsets2[j] = ssao.atrous_offsets[j] * glm::pow(2.0f, i);
          }
          ssaoblur->Set1FloatArray("offsets[0]", offsets2);
          ssaoblur->SetBool("u_horizontal", false);
          glBindTextureUnit(0, ssao.texture);
          glNamedFramebufferTexture(ssao.fbo, GL_COLOR_ATTACHMENT0, ssao.textureBlurred, 0);
          glDrawArrays(GL_TRIANGLES, 0, 3);
          ssaoblur->SetBool("u_horizontal", true);
          glBindTextureUnit(0, ssao.textureBlurred);
          glNamedFramebufferTexture(ssao.fbo, GL_COLOR_ATTACHMENT0, ssao.texture, 0);
          glDrawArrays(GL_TRIANGLES, 0, 3);
        }
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, hdr.fbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindTextureUnit(0, gNormal);
    glBindTextureUnit(1, gAlbedo);
    glBindTextureUnit(2, gRMA);
    glBindTextureUnit(3, gDepth);
    glBindTextureUnit(4, ssao.texture);
    glBindTextureUnit(5, filteredTex);
    glBindTextureUnit(6, irradianceMap);
    glBindTextureUnit(7, envMap_hdri->GetID());

    // global light pass (and apply shadow)
    {
      glDisable(GL_DEPTH_TEST);
      auto& gPhongGlobal = Shader::shaders["gPhongGlobal"];
      gPhongGlobal->Bind();
      gPhongGlobal->SetInt("u_shadowMethod", shadow_method);
      gPhongGlobal->SetFloat("u_C", eConstant);
      gPhongGlobal->SetVec3("u_viewPos", cam.GetPos());
      gPhongGlobal->SetIVec2("u_screenSize", WINDOW_WIDTH, WINDOW_HEIGHT);
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
      glBlitNamedFramebuffer(gfbo, hdr.fbo,
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
      glEnable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);
      glDepthFunc(GL_LEQUAL);
      glBlendFunc(GL_ONE, GL_ZERO);

      auto& hdriShader = Shader::shaders["hdri_skybox"];
      hdriShader->Bind();
      hdriShader->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      hdriShader->SetIVec2("u_screenSize", WINDOW_WIDTH, WINDOW_HEIGHT);
      hdriShader->SetVec3("u_camPos", cam.GetPos());
      envMap_hdri->Bind(0);
      //envMap_irradiance->Bind(0);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // volumetric/crepuscular/god rays pass (write to volumetrics texture)
    if (volumetrics.enabled)
    {
      glViewport(0, 0, volumetrics.framebuffer_width, volumetrics.framebuffer_height);
      glBindFramebuffer(GL_FRAMEBUFFER, volumetrics.fbo);
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
      volumetric->SetIVec2("u_screenSize", volumetrics.framebuffer_width, volumetrics.framebuffer_height);
      volumetric->SetInt("NUM_STEPS", volumetrics.steps);
      volumetric->SetFloat("intensity", volumetrics.intensity);
      volumetric->SetFloat("noiseOffset", volumetrics.noiseOffset);
      volumetric->SetFloat("u_beerPower", volumetrics.beerPower);
      volumetric->SetFloat("u_powderPower", volumetrics.powderPower);
      volumetric->SetFloat("u_distanceScale", volumetrics.distanceScale);
      volumetric->SetFloat("u_heightOffset", volumetrics.heightOffset);
      volumetric->SetFloat("u_hfIntensity", volumetrics.hfIntensity);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      if (volumetrics.atrous_passes > 0)
      {
        //blurTexture16rf(volumetrics.tex, volumetrics.texBlur, volumetrics.framebuffer_width, volumetrics.framebuffer_height, VOLUMETRIC_BLUR_PASSES, VOLUMETRIC_BLUR_STRENGTH);
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, volumetrics.atrousFbo);
        auto& atrousFilter = Shader::shaders["atrous_volumetric"];
        atrousFilter->Bind();
        atrousFilter->SetInt("gColor", 0);
        atrousFilter->SetFloat("c_phi", volumetrics.c_phi);
        atrousFilter->SetFloat("stepwidth", volumetrics.stepWidth);
        atrousFilter->SetIVec2("u_resolution", volumetrics.framebuffer_width, volumetrics.framebuffer_height);
        atrousFilter->Set1FloatArray("kernel[0]", volumetrics.atrouskernel);
        atrousFilter->Set2FloatArray("offsets[0]", volumetrics.atrouskerneloffsets);

        glNamedFramebufferDrawBuffer(volumetrics.atrousFbo, GL_COLOR_ATTACHMENT0);
        glBindTextureUnit(0, volumetrics.tex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (volumetrics.atrous_passes == 2) // two passes (won't support more)
        {
          glm::vec2 atrousKernelOffsets2[25];
          for (int i = 0; i < 25; i++)
          {
            atrousKernelOffsets2[i] = volumetrics.atrouskerneloffsets[i] * 2.0f;
          }
          atrousFilter->Set2FloatArray("offsets[0]", atrousKernelOffsets2);
          glNamedFramebufferDrawBuffer(volumetrics.atrousFbo, GL_COLOR_ATTACHMENT1);
          glBindTextureUnit(0, volumetrics.atrousTex);
          glDrawArrays(GL_TRIANGLES, 0, 3);
        }
      }
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glDepthMask(GL_FALSE);

    // screen-space reflections
    if (ssr.enabled)
    {
      glViewport(0, 0, ssr.framebuffer_width, ssr.framebuffer_height);
      glBindFramebuffer(GL_FRAMEBUFFER, ssr.fbo);
      glClear(GL_COLOR_BUFFER_BIT);
      auto& ssrShader = Shader::shaders["ssr"];
      ssrShader->Bind();
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBindTextureUnit(0, hdr.colorTex);
      glBindTextureUnit(1, gDepth);
      glBindTextureUnit(2, gRMA);
      glBindTextureUnit(3, gNormal);
      ssrShader->SetMat4("u_proj", cam.GetProj());
      ssrShader->SetMat4("u_view", cam.GetView());
      ssrShader->SetMat4("u_invViewProj", glm::inverse(cam.GetViewProj()));
      ssrShader->SetFloat("rayStep", ssr.rayStep);
      ssrShader->SetFloat("minRayStep", ssr.minRayStep);
      ssrShader->SetFloat("thickness", ssr.thickness);
      ssrShader->SetFloat("searchDist", ssr.searchDist);
      ssrShader->SetInt("maxSteps", ssr.maxRaySteps);
      ssrShader->SetInt("binarySearchSteps", ssr.binarySearchSteps);
      ssrShader->SetIVec2("u_viewportSize", ssr.framebuffer_width, ssr.framebuffer_height);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // composite blurred volumetrics and SSR with unprocessed image
    {
      glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
      glBindFramebuffer(GL_FRAMEBUFFER, postprocessFbo);
      glBlendFunc(GL_ONE, GL_ONE);
      glClear(GL_COLOR_BUFFER_BIT);
      auto& fsshader = Shader::shaders["fstexture"];
      fsshader->Bind();
      fsshader->SetInt("u_texture", 0);
      glBindTextureUnit(0, hdr.colorTex);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      if (volumetrics.enabled)
      {
        if (volumetrics.atrous_passes % 2 == 0)
        {
          glBindTextureUnit(0, volumetrics.tex); // 0 or 2 pass a-trous
        }
        else
        {
          glBindTextureUnit(0, volumetrics.atrousTex); // 1 pass a-trous
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
      if (ssr.enabled)
      {
        glBindTextureUnit(0, ssr.tex);
        glBlendFunc(GL_ONE, GL_ONE);
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
    }

    // tone mapping + gamma correction pass
    ApplyTonemapping(dt);

    glBindFramebuffer(GL_FRAMEBUFFER, postprocessFbo);
    glBlendFunc(GL_ONE, GL_ONE);
    glNamedFramebufferTexture(postprocessFbo, GL_COLOR_ATTACHMENT0, legitFinalImage, 0);
    if (fxaa.enabled)
    {
      glBindTextureUnit(0, postprocessPostSRGB);
      auto& fxaaShader = Shader::shaders["fxaa"];
      fxaaShader->Bind();
      fxaaShader->SetVec2("u_invScreenSize", 1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT);
      fxaaShader->SetFloat("u_contrastThreshold", fxaa.contrastThreshold);
      fxaaShader->SetFloat("u_relativeThreshold", fxaa.relativeThreshold);
      fxaaShader->SetFloat("u_pixelBlendStrength", fxaa.pixelBlendStrength);
      fxaaShader->SetFloat("u_edgeBlendStrength", fxaa.edgeBlendStrength);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    else
    {
      drawFSTexture(postprocessPostSRGB);
    }
    glNamedFramebufferTexture(postprocessFbo, GL_COLOR_ATTACHMENT0, postprocessColor, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    drawFSTexture(legitFinalImage);

#pragma region yeah
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
      drawFSTexture(volumetrics.tex);
    }
    if (Input::IsKeyDown(GLFW_KEY_7))
    {
      drawFSTexture(volumetrics.atrousTex);
    }
    if (Input::IsKeyDown(GLFW_KEY_8))
    {
      drawFSTexture(ssr.tex);
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
    if (Input::IsKeyDown(GLFW_KEY_O))
    {
      drawFSTexture(irradianceMap);
    }
    if (Input::IsKeyDown(GLFW_KEY_I))
    {
      drawFSTexture(ssao.texture);
    }
    if (Input::IsKeyDown(GLFW_KEY_U))
    {
      drawFSTexture(ssao.textureBlurred);
    }
    if (Input::IsKeyPressed(GLFW_KEY_C))
    {
      Shader::shaders.erase("fxaa");
      Shader::shaders["fxaa"].emplace(Shader(
        {
          { "fullscreen_tri.vs", GL_VERTEX_SHADER },
          { "fxaa.fs", GL_FRAGMENT_SHADER }
        }));
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
      Shader::shaders.erase("ssao");
      Shader::shaders["ssao"].emplace(Shader(
        {
          { "fullscreen_tri.vs", GL_VERTEX_SHADER },
          { "ssao.fs", GL_FRAGMENT_SHADER }
        }));
    }
#pragma endregion

    if (Input::IsKeyDown(GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(window, true);
    }

    if (cursorVisible)
    {
      DrawUI(dt);
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(window);
  }
}

void Renderer::CreateFramebuffers()
{
  GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
  // create SSAO framebuffer
  glCreateTextures(GL_TEXTURE_2D, 1, &ssao.texture);
  glTextureStorage2D(ssao.texture, 1, GL_R8, WINDOW_WIDTH, WINDOW_HEIGHT);
  glTextureParameteriv(ssao.texture, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  glCreateTextures(GL_TEXTURE_2D, 1, &ssao.textureBlurred);
  glTextureStorage2D(ssao.textureBlurred, 1, GL_R8, WINDOW_WIDTH, WINDOW_HEIGHT);
  glCreateFramebuffers(1, &ssao.fbo);
  glNamedFramebufferTexture(ssao.fbo, GL_COLOR_ATTACHMENT0, ssao.texture, 0);
  glNamedFramebufferDrawBuffer(ssao.fbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(ssao.fbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create SSAO framebuffer");
  }

  // create HDR framebuffer
  glCreateTextures(GL_TEXTURE_2D, 1, &hdr.colorTex);
  glTextureStorage2D(hdr.colorTex, 1, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &hdr.depthTex);
  glTextureStorage2D(hdr.depthTex, 1, GL_DEPTH_COMPONENT32F, WINDOW_WIDTH, WINDOW_HEIGHT);
  glCreateFramebuffers(1, &hdr.fbo);
  glNamedFramebufferTexture(hdr.fbo, GL_COLOR_ATTACHMENT0, hdr.colorTex, 0);
  glNamedFramebufferTexture(hdr.fbo, GL_DEPTH_ATTACHMENT, hdr.depthTex, 0);
  glNamedFramebufferDrawBuffer(hdr.fbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(hdr.fbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create HDR framebuffer");
  }

  // create SSR framebuffer + textures
  glCreateTextures(GL_TEXTURE_2D, 1, &ssr.tex);
  glTextureStorage2D(ssr.tex, 1, GL_RGBA16F, ssr.framebuffer_width, ssr.framebuffer_height);
  glTextureParameteri(ssr.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(ssr.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glCreateTextures(GL_TEXTURE_2D, 1, &ssr.texBlur);
  glTextureStorage2D(ssr.texBlur, 1, GL_RGBA16F, ssr.framebuffer_width, ssr.framebuffer_height);
  glCreateFramebuffers(1, &ssr.fbo);
  glNamedFramebufferTexture(ssr.fbo, GL_COLOR_ATTACHMENT0, ssr.tex, 0);
  glNamedFramebufferDrawBuffer(ssr.fbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(ssr.fbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create SSR framebuffer");
  }

  // create volumetrics texture + fbo + intermediate (for blurring)
  glCreateTextures(GL_TEXTURE_2D, 1, &volumetrics.tex);
  glTextureStorage2D(volumetrics.tex, 1, GL_R16F, volumetrics.framebuffer_width, volumetrics.framebuffer_height);
  glTextureParameteri(volumetrics.tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(volumetrics.tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(volumetrics.tex, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTextureParameteri(volumetrics.tex, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glCreateFramebuffers(1, &volumetrics.fbo);
  glNamedFramebufferTexture(volumetrics.fbo, GL_COLOR_ATTACHMENT0, volumetrics.tex, 0);
  glNamedFramebufferDrawBuffer(volumetrics.fbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(volumetrics.fbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create volumetrics framebuffer");
  }
  glCreateTextures(GL_TEXTURE_2D, 1, &volumetrics.texBlur);
  glTextureStorage2D(volumetrics.texBlur, 1, GL_R16F, volumetrics.framebuffer_width, volumetrics.framebuffer_height);
  glTextureParameteriv(volumetrics.texBlur, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

  // create a-trous fbo and texture
  glCreateTextures(GL_TEXTURE_2D, 1, &volumetrics.atrousTex);
  glTextureStorage2D(volumetrics.atrousTex, 1, GL_R16F, WINDOW_WIDTH, WINDOW_HEIGHT);
  glTextureParameteriv(volumetrics.atrousTex, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  glTextureParameteri(volumetrics.atrousTex, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTextureParameteri(volumetrics.atrousTex, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glCreateFramebuffers(1, &volumetrics.atrousFbo);
  glNamedFramebufferTexture(volumetrics.atrousFbo, GL_COLOR_ATTACHMENT0, volumetrics.atrousTex, 0);
  glNamedFramebufferTexture(volumetrics.atrousFbo, GL_COLOR_ATTACHMENT1, volumetrics.tex, 0);
  glNamedFramebufferDrawBuffer(volumetrics.atrousFbo, GL_COLOR_ATTACHMENT0);
  if (GLenum status = glCheckNamedFramebufferStatus(volumetrics.atrousFbo, GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
  {
    throw std::runtime_error("Failed to create a-trous framebuffer");
  }
  GLint swizzleMaskVolumetric[] = { GL_RED, GL_RED, GL_RED, GL_RED };
  glTextureParameteriv(volumetrics.tex, GL_TEXTURE_SWIZZLE_RGBA, swizzleMaskVolumetric);
  glTextureParameteriv(volumetrics.atrousTex, GL_TEXTURE_SWIZZLE_RGBA, swizzleMaskVolumetric);

  // create tone mapping buffers
  std::vector<int> zeros(hdr.NUM_BUCKETS, 0);
  float expo[] = { hdr.exposureFactor, 0 };
  hdr.exposureBuffer = std::make_unique<StaticBuffer>(expo, 2 * sizeof(float), 0);
  hdr.histogramBuffer = std::make_unique<StaticBuffer>(zeros.data(), zeros.size() * sizeof(int), 0);

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
  glTextureStorage2D(gAlbedo, 1, GL_RGBA8, WINDOW_WIDTH, WINDOW_HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &gNormal);
  glTextureStorage2D(gNormal, 1, GL_RG8_SNORM, WINDOW_WIDTH, WINDOW_HEIGHT);
  //glTextureStorage2D(gNormal, 1, GL_RGBA8_SNORM, WINDOW_WIDTH, WINDOW_HEIGHT); // debugging format
  glCreateTextures(GL_TEXTURE_2D, 1, &gRMA);
  glTextureStorage2D(gRMA, 1, GL_RGB10_A2, WINDOW_WIDTH, WINDOW_HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &gDepth);
  glTextureStorage2D(gDepth, 1, GL_DEPTH_COMPONENT32F, WINDOW_WIDTH, WINDOW_HEIGHT);

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
  glTextureStorage2D(postprocessColor, 1, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT);
  glCreateTextures(GL_TEXTURE_2D, 1, &postprocessPostSRGB);
  glTextureStorage2D(postprocessPostSRGB, 1, GL_RGBA8, WINDOW_WIDTH, WINDOW_HEIGHT);
  glTextureParameteri(postprocessPostSRGB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(postprocessPostSRGB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glCreateTextures(GL_TEXTURE_2D, 1, &legitFinalImage);
  glTextureStorage2D(legitFinalImage, 1, GL_RGBA8, WINDOW_WIDTH, WINDOW_HEIGHT);
  glTextureParameteri(legitFinalImage, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(legitFinalImage, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
  sphere2 = std::move(LoadObjMesh("Resources/Models/sphere2.obj", materialManager)[0]);

  LoadScene1();
}

void Renderer::Cleanup()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glDeleteVertexArrays(1, &vao);

  // do not gaze at it too closely
  glDeleteTextures(1, &volumetrics.tex);
  glDeleteTextures(1, &volumetrics.texBlur);
  glDeleteFramebuffers(1, &volumetrics.fbo);

  glDeleteTextures(1, &volumetrics.atrousTex);
  glDeleteFramebuffers(1, &volumetrics.atrousFbo);

  glDeleteTextures(1, &gAlbedo);
  glDeleteTextures(1, &gNormal);
  glDeleteTextures(1, &gDepth);
  glDeleteTextures(1, &gRMA);
  glDeleteFramebuffers(1, &gfbo);

  glDeleteTextures(1, &postprocessColor);
  glDeleteFramebuffers(1, &postprocessFbo);

  glDeleteTextures(1, &ssr.tex);
  glDeleteTextures(1, &ssr.texBlur);
  glDeleteFramebuffers(1, &ssr.fbo);

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

  glDeleteTextures(1, &hdr.colorTex);
  glDeleteTextures(1, &hdr.depthTex);
  glDeleteFramebuffers(1, &hdr.fbo);

  glDeleteFramebuffers(1, &ssao.fbo);
  glDeleteTextures(1, &ssao.texture);
  glDeleteTextures(1, &ssao.textureBlurred);

  glDeleteTextures(1, &irradianceMap);

  glfwDestroyWindow(window);
  glfwTerminate();
}

void Renderer::DrawUI(float dt)
{
  ImGui::Begin("Scene");

  static double frameTimeExp = 0;
  static double alpha = .01;

  frameTimeExp = alpha * dt + (1.0 - alpha) * frameTimeExp;
  alpha = glm::clamp((float)dt, 0.0f, 1.0f);

  ImGui::Text("FPS: %.0f (%.2f ms)", 1.f / frameTimeExp, frameTimeExp * 1000);
  ImGui::SameLine();
  if (ImGui::Checkbox("vsync", &vsyncEnabled))
  {
    glfwSwapInterval(vsyncEnabled);
  }
  if (ImGui::SliderFloat("FoV", &fovDeg, 30.0f, 100.0f))
  {
    cam.SetFoV(fovDeg);
    cam.Update(0);
  }

  if (ImGui::TreeNode("Environment"))
  {
    ImGui::Text("Global Light");
    {
      if (ImGui::SliderAngle("Sun angle", &sunPosition, 180, 360))
      {
        globalLight.direction.x = .1f;
        globalLight.direction.y = glm::sin(sunPosition);
        globalLight.direction.z = glm::cos(sunPosition);
        globalLight.direction = glm::normalize(globalLight.direction);
      }
      ImGui::ColorEdit3("Diffuse", &globalLight.diffuse[0]);
    }

    ImGui::Separator();
    ImGui::Text("PBR/IBL");
    {
      ImGui::Checkbox("Material Override", &materialOverride);
      ImGui::Checkbox("Override AO", &AOoverride);
      ImGui::Checkbox("Draw PBR Grid", &drawPbrSphereGridQuestionMark);
      ImGui::SliderInt("Num Env Samples", &numEnvSamples, 1, 100);
      ImGui::ColorEdit3("Albedo Override", &albedoOverride[0]);
      ImGui::SliderFloat("Roughness Override", &roughnessOverride, 0.0f, 1.0f);
      ImGui::SliderFloat("Metalness Override", &metalnessOverride, 0.0f, 1.0f);
      ImGui::SliderFloat("AO Override", &ambientOcclusionOverride, 0.0f, 1.0f);
    }
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Scene-Data"))
  {
    ImGui::SliderInt("Local lights", &numLights, 0, 40000);
    ImGui::SliderFloat("Light Cutoff", &lightVolumeThreshold, 0.001f, 0.1f);
    if (ImGui::SliderFloat2("Light Falloff", glm::value_ptr(lightFalloff), 0.01f, 10.0f, "%.3f", 2.0f))
    {
      if (lightFalloff.y < lightFalloff.x)
      {
        std::swap(lightFalloff.x, lightFalloff.y);
      }
    }

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

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Environment Map"))
  {
    for (auto& p : fss::directory_iterator("Resources/IBL"))
    {
      std::string str = p.path().string();
      if (str.find(".irr") == std::string::npos && ImGui::Button(str.c_str()))
      {
        LoadEnvironmentMap(str);
      }
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("View Texture"))
  {
    ImGui::Image((void*)uiViewBuffer, ImVec2(300, 300), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::RadioButton("gAlbedo", &uiViewBuffer, gAlbedo);
    ImGui::RadioButton("gNormal", &uiViewBuffer, gNormal);
    ImGui::RadioButton("gDepth", &uiViewBuffer, gDepth);
    ImGui::RadioButton("gRMA", &uiViewBuffer, gRMA);
    ImGui::RadioButton("hdr.colorTex", &uiViewBuffer, hdr.colorTex);
    ImGui::RadioButton("hdr.depthTex", &uiViewBuffer, hdr.depthTex);
    ImGui::RadioButton("shadowDepthGoodFormat", &uiViewBuffer, vshadowDepthGoodFormat);
    ImGui::RadioButton("volumetrics.atrousTex", &uiViewBuffer, volumetrics.atrousTex);
    ImGui::RadioButton("volumetrics.tex", &uiViewBuffer, volumetrics.tex);
    ImGui::RadioButton("postprocessColor", &uiViewBuffer, postprocessColor);
    ImGui::RadioButton("ssr.tex", &uiViewBuffer, ssr.tex);

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Lights List"))
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

      ImGui::Separator();
    }
    ImGui::TreePop();
  }

  ImGui::End();


  ImGui::Begin("Features");

  ImGui::Checkbox("##volumetric", &volumetrics.enabled);
  ImGui::SameLine();
  if (ImGui::TreeNode("Volumetric Fog"))
  {
    ImGui::Text("Rays");
    ImGui::Separator();
    ImGui::SliderInt("Steps", &volumetrics.steps, 1, 100);
    ImGui::SliderFloat("Intensity", &volumetrics.intensity, 0.0f, 1.0f, "%.3f", 3.0f);
    ImGui::SliderFloat("Beer Power", &volumetrics.beerPower, 0.0f, 3.0f);
    ImGui::SliderFloat("Powder Power", &volumetrics.powderPower, 0.0f, 3.0f);
    ImGui::SliderFloat("Offset", &volumetrics.noiseOffset, 0.0f, 1.0f);

    ImGui::Text("Height Fog");
    ImGui::SliderFloat("Distance Scale", &volumetrics.distanceScale, 0.001f, 100.0f);
    ImGui::SliderFloat("Height Offset", &volumetrics.heightOffset, -10.0f, 10.0f);
    ImGui::SliderFloat("hfIntensity", &volumetrics.hfIntensity, 0.0f, 1.0f);

    ImGui::Text((const char*)(u8"À-Trous"));
    ImGui::Separator();
    ImGui::Text("Passes");
    int passes = volumetrics.atrous_passes;
    ImGui::RadioButton("Zero", &passes, 0);
    ImGui::SameLine(); ImGui::RadioButton("One", &passes, 1);
    ImGui::SameLine(); ImGui::RadioButton("Two", &passes, 2);
    volumetrics.atrous_passes = passes;
    ImGui::SliderFloat("c_phi", &volumetrics.c_phi, .0001f, 10.0f, "%.4f", 4.0f);
    ImGui::SliderFloat("Step Width", &volumetrics.stepWidth, 0.5f, 2.0f, "%.3f");

    ImGui::TreePop();
  }

  ImGui::Checkbox("##ssr", &ssr.enabled);
  ImGui::SameLine();
  if (ImGui::TreeNode("Screen-Space Reflections"))
  {
    ImGui::SliderFloat("Step size", &ssr.rayStep, 0.01f, 1.0f);
    ImGui::SliderFloat("Min step", &ssr.minRayStep, 0.01f, 1.0f);
    ImGui::SliderFloat("Thickness", &ssr.thickness, 0.00f, 1.0f);
    ImGui::SliderFloat("Search distance", &ssr.searchDist, 1.0f, 50.0f);
    ImGui::SliderInt("Max steps", &ssr.maxRaySteps, 0, 100);
    ImGui::SliderInt("Binary search steps", &ssr.binarySearchSteps, 0, 10);

    ImGui::TreePop();
  }

  ImGui::Checkbox("##ssao", &ssao.enabled);
  ImGui::SameLine();
  if (ImGui::TreeNode("SSAO"))
  {
    ImGui::SliderInt("Samples", &ssao.samples_near, 0, 30);
    ImGui::SliderFloat("Delta", &ssao.delta, 0.0001f, 0.01f, "%.4f", 2.0f);
    ImGui::SliderFloat("Range", &ssao.range, 0.1f, 3.0f);
    ImGui::SliderFloat("s", &ssao.s, 0.1f, 3.0f);
    ImGui::SliderFloat("k", &ssao.k, 0.1f, 3.0f);
    ImGui::Text("A-Trous");
    ImGui::Separator();
    ImGui::SliderInt("Passes", &ssao.atrous_passes, 0, 5);
    ImGui::SliderFloat("n_phi", &ssao.atrous_n_phi, .001f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("p_phi", &ssao.atrous_p_phi, .001f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Step Width", &ssao.atrous_step_width, 0.5f, 2.0f, "%.3f");
    ImGui::TreePop();
  }

  ImGui::Checkbox("##fxaa", &fxaa.enabled);
  ImGui::SameLine();
  if (ImGui::TreeNode("FXAA"))
  {
    ImGui::SliderFloat("Abs. Threshold", &fxaa.contrastThreshold, .03f, .09f);
    ImGui::SliderFloat("Rel. Threshold", &fxaa.relativeThreshold, .125f, .25f);
    ImGui::SliderFloat("Pixel Blend", &fxaa.pixelBlendStrength, 0.0f, 1.0f);
    ImGui::SliderFloat("Edge Blend", &fxaa.edgeBlendStrength, 0.0f, 1.0f);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Shadows"))
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

    ImGui::SliderInt("Blur Passes", &BLUR_PASSES, 0, 5);
    ImGui::SliderInt("Blur Width", &BLUR_STRENGTH, 0, 6);
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

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Postprocessing"))
  {
    ImGui::SliderFloat("Lum Target", &hdr.targetLuminance, 0.01f, 1.0f);
    ImGui::SliderFloat("Exposure Factor", &hdr.exposureFactor, 0.01f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Adjustment Speed", &hdr.adjustmentSpeed, 0.0f, 10.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Min Exposure", &hdr.minExposure, 0.01f, 100.0f, "%.3f", 2.0f);
    ImGui::SliderFloat("Max Exposure", &hdr.maxExposure, 0.01f, 100.0f, "%.3f", 2.0f);

    ImGui::TreePop();
  }

  ImGui::End();

  ImGui::Begin(("Magnifier: " + std::string(magnifierLock ? "Locked" : "Unlocked") + "###mag").c_str());
  ImGui::SliderFloat("Scale", &magnifierScale, 0.002f, 0.1f, "%.4f", 2.0f);
  
  glm::vec2 mp = magnifierLock ? magnifier_lastMouse : Input::GetScreenPos();
  magnifier_lastMouse = mp;
  mp.y = WINDOW_HEIGHT - mp.y;
  mp /= glm::vec2(WINDOW_WIDTH, WINDOW_HEIGHT);
  float ar = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
  glm::vec2 uv0{ mp.x - magnifierScale, mp.y + magnifierScale * ar };
  glm::vec2 uv1{ mp.x + magnifierScale, mp.y - magnifierScale * ar };
  uv0 = glm::clamp(uv0, glm::vec2(0), glm::vec2(1));
  uv1 = glm::clamp(uv1, glm::vec2(0), glm::vec2(1));
  ImGui::Image((void*)legitFinalImage, ImVec2(300, 300), ImVec2(uv0.x, uv0.y), ImVec2(uv1.x, uv1.y));
  ImGui::End();
}

void Renderer::ApplyTonemapping(float dt)
{
  glDisable(GL_BLEND);

  glBindTextureUnit(1, postprocessColor);

  const float logLowLum = glm::log(hdr.targetLuminance / hdr.maxExposure);
  const float logMaxLum = glm::log(hdr.targetLuminance / hdr.minExposure);
  const int computePixelsX = WINDOW_WIDTH / 2;
  const int computePixelsY = WINDOW_HEIGHT / 2;

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
    hdr.histogramBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 0);
    glDispatchCompute(xgroups, ygroups, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }

  //float expo{};
  //glGetNamedBufferSubData(hdr.exposureBuffer, 0, sizeof(float), &expo);
  //printf("Exposure: %f\n", expo);

  {
    hdr.exposureBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 0);
    hdr.histogramBuffer->BindBase(GL_SHADER_STORAGE_BUFFER, 1);
    auto& cshdr = Shader::shaders["calc_exposure"];
    cshdr->Bind();
    cshdr->SetFloat("u_dt", dt);
    cshdr->SetFloat("u_adjustmentSpeed", hdr.adjustmentSpeed);
    cshdr->SetFloat("u_logLowLum", logLowLum);
    cshdr->SetFloat("u_logMaxLum", logMaxLum);
    cshdr->SetFloat("u_targetLuminance", hdr.targetLuminance);
    cshdr->SetInt("u_numPixels", computePixelsX * computePixelsY);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  auto& shdr = Shader::shaders["tonemap"];
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
  shdr->Bind();
  shdr->SetFloat("u_exposureFactor", hdr.exposureFactor);
  shdr->SetInt("u_hdrBuffer", 1);
  glNamedFramebufferTexture(postprocessFbo, GL_COLOR_ATTACHMENT0, postprocessPostSRGB, 0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glNamedFramebufferTexture(postprocessFbo, GL_COLOR_ATTACHMENT0, postprocessColor, 0);
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

  //PointLight light;
  //light.diffuse = glm::vec4(5.f, 5.f, 5.f, 0.f);
  //light.position = glm::vec4(2, 3, 0, 0);
  //light.linear = 0;
  //light.quadratic = 1;
  //light.radiusSquared = light.CalcRadiusSquared(lightVolumeThreshold);
  //localLights.push_back(light);

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

  LoadEnvironmentMap("Resources/IBL/14-Hamarikyu_Bridge_B_3k.hdr");

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

  LoadEnvironmentMap("Resources/IBL/Arches_E_PineTree_3k.hdr");

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

void Renderer::LoadEnvironmentMap(std::string path)
{
  TextureCreateInfo info
  {
    .path = path,
    .sRGB = false,
    .generateMips = true,
    .HDR = true,
    .minFilter = GL_LINEAR_MIPMAP_LINEAR,
    .magFilter = GL_LINEAR,
  };

  envMap_hdri = std::make_unique<Texture2D>(info);

  GLuint samplerID{};
  glCreateSamplers(1, &samplerID);
  glSamplerParameteri(samplerID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glSamplerParameteri(samplerID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glSamplerParameteri(samplerID, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glSamplerParameteri(samplerID, GL_TEXTURE_WRAP_T, GL_REPEAT);
  //glSamplerParameterf(samplerID, GL_TEXTURE_MIN_LOD, 8.f);
  //glSamplerParameterf(samplerID, GL_TEXTURE_LOD_BIAS, 1.0f);
  glBindSampler(0, samplerID);

  float aspectRatio = (float)envMap_hdri->GetSize().x / envMap_hdri->GetSize().y;
  glm::ivec2 dim;
  dim.x = aspectRatio * 512;
  dim.y = dim.x / aspectRatio;
  if (irradianceMap)
  {
    glDeleteTextures(1, &irradianceMap);
  }
  glCreateTextures(GL_TEXTURE_2D, 1, &irradianceMap);
  GLuint levels = (GLuint)glm::ceil(glm::log2((float)glm::min(dim.x, dim.y)));
  glTextureParameteri(irradianceMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTextureParameteri(irradianceMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTextureParameterf(irradianceMap, GL_TEXTURE_MIN_LOD, 5.5f);
  //glTextureParameterf(irradianceMap, GL_TEXTURE_LOD_BIAS, 6.0f);
  glTextureStorage2D(irradianceMap, levels, GL_RGBA16F, dim.x, dim.y);

  convolve_image(envMap_hdri->GetID(), irradianceMap, dim.x, dim.y);
  glGenerateTextureMipmap(irradianceMap);

  glBindSampler(0, 0);
  glDeleteSamplers(1, &samplerID);
}

void Renderer::DrawPbrSphereGrid()
{
  auto& gbufBindless = Shader::shaders["gBufferBindless"];
  gbufBindless->Bind();
  gbufBindless->SetMat4("u_viewProj", cam.GetViewProj());

  // draw PBR sphere grid
  glVertexArrayVertexBuffer(vao, 0, sphere2.GetVBOID(), 0, sizeof(Vertex));
  glVertexArrayElementBuffer(vao, sphere2.GetEBOID());
  gbufBindless->SetBool("u_materialOverride", true);
  gbufBindless->SetVec3("u_albedoOverride", albedoOverride);

  int gridsize = 4;
  for (int x = 0; x < gridsize; x++)
  {
    for (int y = 0; y < gridsize; y++)
    {
      Transform transform;
      transform.translation = glm::vec3(x * 2, y * 2, 5);
      transform.scale = glm::vec3(.0125);
      ObjectUniforms uniform
      {
        .modelMatrix = transform.GetModelMatrix(),
        .materialIndex = 0
      };
      StaticBuffer uniformBuffer2(&uniform, sizeof(ObjectUniforms), 0);
      uniformBuffer2.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
      gbufBindless->SetFloat("u_roughnessOverride", (float)x / (gridsize - 1));
      gbufBindless->SetFloat("u_metalnessOverride", (float)y / (gridsize - 1));
      glDrawElements(GL_TRIANGLES, sphere2.GetVertexCount(), GL_UNSIGNED_INT, nullptr);
    }
  }
}