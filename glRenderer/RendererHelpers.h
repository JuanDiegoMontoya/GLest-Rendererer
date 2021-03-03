#pragma once
#include <string>
#include <span>

void GLAPIENTRY
GLerrorCB(GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  GLsizei length,
  const GLchar* message,
  const void* userParam);

void CompileShaders();
void drawFSTexture(GLuint texID);

void blurTextureBase(GLuint inOutTex, GLuint intermediateTexture,
  GLint width, GLint height, GLint passes, GLint strength, std::span<std::string> shaderNames, GLenum imageFormat);
void blurTextureRG32f(GLuint inOutTex, GLuint intermediateTexture, 
  GLint width, GLint height, GLint passes, GLint strength);
void blurTextureR32f(GLuint inOutTex, GLuint intermediateTexture,
  GLint width, GLint height, GLint passes, GLint strength);
void blurTextureRGBA32f(GLuint inOutTex, GLuint intermediateTexture,
  GLint width, GLint height, GLint passes, GLint strength);
void blurTextureRGBA16f(GLuint inOutTex, GLuint intermediateTexture, 
  GLint width, GLint height, GLint passes, GLint strength);

struct DrawElementsIndirectCommand
{
  GLuint count;
  GLuint primCount;
  GLuint firstIndex;
  GLuint baseVertex;
  GLuint baseInstance;
};