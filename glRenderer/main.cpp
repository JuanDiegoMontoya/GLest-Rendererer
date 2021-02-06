#include "Renderer.h"
#include <iostream>
#include <exception>

int main()
{
  Renderer app;

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

// TODO list (unordered)
// skybox
// image-based lighting
// imgui
// tiled/clustered shading
// efficient draw call submission/multi draw
// PBR

// DONE
// VSM