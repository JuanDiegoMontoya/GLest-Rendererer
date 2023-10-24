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

export struct TextureCreateInfo
{
  std::string path;
  bool sRGB{};
  bool generateMips{};
  bool HDR{};
  int minFilter{};
  int magFilter{};
};

export class Texture2D
{
public:
  Texture2D(const TextureCreateInfo& createInfo);
  Texture2D(const Texture2D& rhs) = delete;
  Texture2D& operator=(Texture2D&& rhs) noexcept;
  Texture2D(Texture2D&& rhs) noexcept;
  ~Texture2D();

  void Bind(unsigned slot = 0) const;
  unsigned GetID() const { return id_; }
  uint64_t GetBindlessHandle() const { return bindlessHandle_; }
  bool Valid() const { return id_ != 0; }

  glm::ivec2 GetSize() const { return dim_; }

private:
  unsigned id_{};
  uint64_t bindlessHandle_{};
  glm::ivec2 dim_{};
};

Texture2D::Texture2D(const TextureCreateInfo& createInfo)
{
  assert(!(createInfo.sRGB && createInfo.HDR)); // cannot have both sRGB and HDR
  stbi_set_flip_vertically_on_load(true);

  std::string tex = createInfo.path;
  bool hasTex = std::filesystem::exists(tex) && std::filesystem::is_regular_file(tex);
  if (hasTex == false)
  {
    //std::cout << "Failed to load texture " << path << ", using fallback.\n";
    return;
    //tex = tex + "error.png";
  }

  int n;
  auto pixels = stbi_loadf(tex.c_str(), &dim_.x, &dim_.y, &n, 4);
  assert(pixels != nullptr);

  glCreateTextures(GL_TEXTURE_2D, 1, &id_);

  // sets the anisotropic filtering texture paramter to the highest supported by the system
  GLfloat a;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &a);
  glTextureParameterf(id_, GL_TEXTURE_MAX_ANISOTROPY, a);

  glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, createInfo.minFilter);
  glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, createInfo.magFilter);
  glTextureParameteri(id_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(id_, GL_TEXTURE_WRAP_T, GL_REPEAT);
  GLuint levels = 1;
  if (createInfo.generateMips)
  {
    levels = (GLuint)glm::ceil(glm::log2((float)glm::min(dim_.x, dim_.y)));
  }
  
  const GLenum internalFormat = createInfo.sRGB ? GL_SRGB8_ALPHA8 : createInfo.HDR ? GL_RGBA16F : GL_RGBA8;
  glTextureStorage2D(id_, levels, internalFormat, dim_.x, dim_.y);
  glTextureSubImage2D(
    id_,
    0,              // mip level 0
    0, 0,           // image start layer
    dim_.x, dim_.y, // x, y size
    GL_RGBA,
    GL_FLOAT,
    pixels);

  stbi_image_free(pixels);

  // use OpenGL to generate mipmaps for us
  if (createInfo.generateMips)
  {
    glGenerateTextureMipmap(id_);
  }

  bindlessHandle_ = glGetTextureHandleARB(id_);
  //Wstd::cout << bindlessHandle_ << '\n';
  glMakeTextureHandleResidentARB(bindlessHandle_);
}

Texture2D& Texture2D::operator=(Texture2D&& rhs) noexcept
{
  if (&rhs == this) return *this;
  return *new (this) Texture2D(std::move(rhs));
}

Texture2D::Texture2D(Texture2D&& rhs) noexcept
{
  this->id_ = std::exchange(rhs.id_, 0);
  this->dim_ = rhs.dim_;
}

Texture2D::~Texture2D()
{
  glDeleteTextures(1, &id_);
}

void Texture2D::Bind(unsigned slot) const
{
  glBindTextureUnit(slot, id_);
}