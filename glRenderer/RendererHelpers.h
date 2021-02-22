#pragma once

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

void blurTexture32rgf(GLuint inOutTex, GLuint intermediateTexture, 
  GLint width, GLint height, GLint passes, GLint strength);
void blurTexture32rf(GLuint inOutTex, GLuint intermediateTexture,
  GLint width, GLint height, GLint passes, GLint strength);
