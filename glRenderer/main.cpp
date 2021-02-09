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
// fix normal mapping
// skybox
// image-based lighting
// tiled/clustered shading
// efficient draw call submission/multi draw
// PBR

// DONE
// imgui
// VSM