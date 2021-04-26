#include "Renderer.h"
#include <iostream>
#include <exception>

int main()
{
  Renderer app;

  try
  {
    app.Run();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// TODO list (unordered)
// PCF
// tiled/clustered shading
// complex SSR (for very shiny surfaces, fallback to IBL if no surface hit)
// fix normal mapping (again)
// more flexible model loader (assimp or similar)
// RSM?
// TAA?

// DONE
// imgui
// SSR
// VSM
// ESM
// fix normal mapping
// use index buffer
// MSM
// skybox
// efficient draw call submission/multi draw
// bindless texturing
// PBR
// image-based lighting
// SSAO
// FXAA