module;

#include <string>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <iostream>
#include <filesystem>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

export module GPU.Texture;

export class Texture2D
{
public:
  Texture2D(std::string_view path, bool isSRGB, bool genMips);
  Texture2D(const Texture2D& rhs) = delete;
  Texture2D& operator=(Texture2D&& rhs) noexcept;
  Texture2D(Texture2D&& rhs) noexcept;
  ~Texture2D();

  void Bind(unsigned slot = 0) const;
  unsigned GetID() const { return rendererID_; }
  uint64_t GetBindlessHandle() const { return bindlessHandle_; }
  bool Valid() const { return rendererID_ != 0; }

  glm::ivec2 GetDimensions() const { return dim_; }

private:
  unsigned rendererID_{};
  uint64_t bindlessHandle_{};
  glm::ivec2 dim_{};
};


Texture2D::Texture2D(std::string_view path, bool isSRGB, bool genMips)
{
  stbi_set_flip_vertically_on_load(true);

  std::string tex = std::string(path);
  bool hasTex = std::filesystem::exists(tex) && std::filesystem::is_regular_file(tex);
  if (hasTex == false)
  {
    //std::cout << "Failed to load texture " << path << ", using fallback.\n";
    return;
    //tex = tex + "error.png";
  }

  int n;
  auto pixels = (unsigned char*)stbi_load(tex.c_str(), &dim_.x, &dim_.y, &n, 4);
  assert(pixels != nullptr);

  glCreateTextures(GL_TEXTURE_2D, 1, &rendererID_);

  // sets the anisotropic filtering texture paramter to the highest supported by the system
  GLfloat a;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &a);
  glTextureParameterf(rendererID_, GL_TEXTURE_MAX_ANISOTROPY, a);

  glTextureParameteri(rendererID_, GL_TEXTURE_MIN_FILTER, genMips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTextureParameteri(rendererID_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(rendererID_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(rendererID_, GL_TEXTURE_WRAP_T, GL_REPEAT);

  GLuint levels = 1;
  if (genMips)
  {
    levels = (GLuint)glm::ceil(glm::log2((float)glm::min(dim_.x, dim_.y)));
  }
  glTextureStorage2D(rendererID_, levels, isSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8, dim_.x, dim_.y);
  glTextureSubImage2D(
    rendererID_,
    0,              // mip level 0
    0, 0,           // image start layer
    dim_.x, dim_.y, // x, y, z size (z = 1 b/c it's just a single layer)
    GL_RGBA,
    GL_UNSIGNED_BYTE,
    pixels);

  stbi_image_free(pixels);

  // use OpenGL to generate mipmaps for us
  if (genMips)
  {
    glGenerateTextureMipmap(rendererID_);
  }

  bindlessHandle_ = glGetTextureHandleARB(rendererID_);
  glMakeTextureHandleResidentARB(bindlessHandle_);
}

Texture2D& Texture2D::operator=(Texture2D&& rhs) noexcept
{
  if (&rhs == this) return *this;
  return *new (this) Texture2D(std::move(rhs));
}

Texture2D::Texture2D(Texture2D&& rhs) noexcept
{
  this->rendererID_ = std::exchange(rhs.rendererID_, 0);
  this->dim_ = rhs.dim_;
}

Texture2D::~Texture2D()
{
  glDeleteTextures(1, &rendererID_);
}

void Texture2D::Bind(unsigned slot) const
  {
    glBindTextureUnit(slot, rendererID_);
  }