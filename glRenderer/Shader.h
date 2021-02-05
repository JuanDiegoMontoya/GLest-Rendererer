#pragma once
#include <glad/glad.h>
#include <unordered_map>
#include <optional>
#include <span>
#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>

struct ShaderInfo
{
  ShaderInfo(std::string p, GLenum t, std::vector<std::pair<std::string, std::string>> r = {}) : path(p), type(t), replace(r) {}

  std::string path{};
  GLenum type{};
  std::vector<std::pair<std::string, std::string>> replace{};
};

// encapsulates shaders by storing uniforms and its GPU memory location
// also stores the program's name and both shader paths for recompiling
class Shader
{
public:
  // universal SPIR-V constructor (takes a list of paths and shader types)
  Shader(std::vector<ShaderInfo> shaders);

  // default constructor (currently no uses)
  Shader()
  {
    programID = 0;
  }

  // move constructor
  Shader(Shader&& other) noexcept : Uniforms(std::move(other.Uniforms))
  {
    this->programID = std::exchange(other.programID, 0);
  }

  ~Shader()
  {
    if (programID != 0)
      glDeleteProgram(programID);
  }

  // Set the active shader to this one
  void Bind() const
  {
    glUseProgram(programID);
  }

  void SetBool(std::string uniform, bool value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1i(programID, Uniforms[uniform], static_cast<int>(value));
  }
  void SetInt(std::string uniform, int value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1i(programID, Uniforms[uniform], value);
  }
  void SetUInt(std::string uniform, unsigned int value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1ui(programID, Uniforms[uniform], value);
  }
  void SetFloat(std::string uniform, float value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1f(programID, Uniforms[uniform], value);
  }
  void Set1FloatArray(std::string uniform, std::span<const float> value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1fv(programID, Uniforms[uniform], value.size(), value.data());
  }
  void Set1FloatArray(std::string uniform, const float* value, GLsizei count)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1fv(programID, Uniforms[uniform], count, value);
  }
  void Set2FloatArray(std::string uniform, std::span<const glm::vec2> value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform2fv(programID, Uniforms[uniform], value.size(), glm::value_ptr(value.front()));
  }
  void Set3FloatArray(std::string uniform, std::span<const glm::vec3> value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform3fv(programID, Uniforms[uniform], value.size(), glm::value_ptr(value.front()));
  }
  void Set4FloatArray(std::string uniform, std::span<const glm::vec4> value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform4fv(programID, Uniforms[uniform], value.size(), glm::value_ptr(value.front()));
  }
  void SetIntArray(std::string uniform, std::span<const int> value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform1iv(programID, Uniforms[uniform], value.size(), value.data());
  }
  void SetVec2(std::string uniform, const glm::vec2& value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform2fv(programID, Uniforms[uniform], 1, glm::value_ptr(value));
  }
  void SetVec2(std::string uniform, float x, float y)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform2f(programID, Uniforms[uniform], x, y);
  }
  void SetIVec2(std::string uniform, const glm::ivec2& value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform2iv(programID, Uniforms[uniform], 1, glm::value_ptr(value));
  }
  void SetIVec2(std::string uniform, int x, int y)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform2i(programID, Uniforms[uniform], x, y);
  }
  void SetVec3(std::string uniform, const glm::vec3& value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform3fv(programID, Uniforms[uniform], 1, glm::value_ptr(value));
  }
  void SetVec3(std::string uniform, float x, float y, float z)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform3f(programID, Uniforms[uniform], x, y, z);
  }
  void SetVec4(std::string uniform, const glm::vec4& value)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform4fv(programID, Uniforms[uniform], 1, glm::value_ptr(value));
  }
  void SetVec4(std::string uniform, float x, float y, float z, float w)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniform4f(programID, Uniforms[uniform], x, y, z, w);
  }
  void SetMat3(std::string uniform, const glm::mat3& mat)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniformMatrix3fv(programID, Uniforms[uniform], 1, GL_FALSE, glm::value_ptr(mat));
  }
  void SetMat4(std::string uniform, const glm::mat4& mat)
  {
    assert(Uniforms.find(uniform) != Uniforms.end());
    glProgramUniformMatrix4fv(programID, Uniforms[uniform], 1, GL_FALSE, glm::value_ptr(mat));
  }

  // list of all shader programs
  static inline std::unordered_map<std::string, std::optional<Shader>> shaders;
private:
  std::unordered_map<std::string, GLint> Uniforms;
  GLuint programID{ 0 };

  using shaderType = GLenum;
  friend class IncludeHandler;

  // shader dir includes source and headers alike
  static constexpr const char* shader_dir_ = "./Resources/Shaders/";
  static std::string loadFile(std::string path);

  GLint compileShader(shaderType type, const std::vector<std::string>& src, std::string_view path);
  void initUniforms();
  void checkLinkStatus(std::vector<std::string_view> files);

  // returns compiled SPIR-V
  std::vector<uint32_t>
    spvPreprocessAndCompile(
      shaderc::Compiler& compiler,
      const shaderc::CompileOptions options,
      const std::vector<std::pair<std::string, std::string>>& replace,
      std::string path,
      shaderc_shader_kind a);

};