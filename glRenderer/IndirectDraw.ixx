module;

#include <glad/glad.h>

export module GPU.IndirectDraw;

export struct DrawElementsIndirectCommand
{
  GLuint count{ 0 };        // num indices in draw call
  GLuint instanceCount{ 0 };// num instances in draw call
  GLuint firstIndex{ 0 };   // offset in index buffer: sizeof(element)*firstIndex from start of buffer
  GLuint baseVertex{ 0 };   // offset in vertex buffer: sizeof(vertex)*baseVertex from start of buffer
  GLuint baseInstance{ 0 }; // first instance to draw (position in instanced buffer)
};

export struct DrawArraysIndirectCommand
{
  GLuint count{ 0 };
  GLuint instanceCount{ 0 };
  GLuint first{ 0 };        // equivalent to baseVertex from previous struct
  GLuint baseInstance{ 0 };
};