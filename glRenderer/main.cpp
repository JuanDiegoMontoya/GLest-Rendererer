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
// image-based lighting
// tiled/clustered shading
// PBR
// correctly account for shininess term in SSR
// fix normal mapping (again)

// DONE
// imgui
// VSM
// ESM
// fix normal mapping
// use index buffer
// MSM
// skybox
// efficient draw call submission/multi draw
// bindless texturing