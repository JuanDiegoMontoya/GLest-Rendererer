#pragma once
#include <string>
#include <glm/glm.hpp>

class Texture2D
{
public:
  Texture2D(std::string_view path);
  Texture2D(const Texture2D& rhs) = delete;
  Texture2D& operator=(Texture2D&& rhs) noexcept;
  Texture2D(Texture2D&& rhs) noexcept;
  ~Texture2D();

  void Bind(unsigned slot = 0) const;
  unsigned GetID() const { return rendererID_; }
  bool Valid() const { return rendererID_ != 0; }

  glm::ivec2 GetDimensions() const { return dim_; }

private:
  unsigned rendererID_ = 0;
  glm::ivec2 dim_{};
};