#include "Shader.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <regex>

class IncludeHandler : public shaderc::CompileOptions::IncluderInterface
{
public:
  virtual shaderc_include_result* GetInclude(
    const char* requested_source,
    shaderc_include_type type,
    const char* requesting_source,
    size_t include_depth)
  {
    auto* data = new shaderc_include_result;
    
    content = new std::string(Shader::loadFile(requested_source));
    source_name = new std::string(requested_source);

    data->content = content->c_str();
    data->source_name = source_name->c_str();
    data->content_length = content->size();
    data->source_name_length = source_name->size();
    data->user_data = nullptr;

    return data;
  }

  virtual void ReleaseInclude(shaderc_include_result* data)
  {
    // hopefully this isn't dumb
    delete content;
    delete source_name;
    delete data;
  }

private:
  std::string* content{};
  std::string* source_name{};
};


Shader::Shader(std::vector<ShaderInfo> shaders)
{
  const std::unordered_map<GLenum, shaderc_shader_kind> gl2shadercTypes =
  {
    { GL_VERTEX_SHADER, shaderc_vertex_shader },
    { GL_FRAGMENT_SHADER, shaderc_fragment_shader },
    { GL_TESS_CONTROL_SHADER, shaderc_tess_control_shader },
    { GL_TESS_EVALUATION_SHADER, shaderc_tess_evaluation_shader },
    { GL_GEOMETRY_SHADER, shaderc_geometry_shader },
    { GL_COMPUTE_SHADER, shaderc_compute_shader }
  };

  shaderc::Compiler compiler;
  assert(compiler.IsValid());

  shaderc::CompileOptions options;
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_opengl, 450);
  options.SetIncluder(std::make_unique<IncludeHandler>());
  options.SetWarningsAsErrors();
  options.SetAutoMapLocations(true);
  options.SetAutoBindUniforms(true);

  std::vector<GLuint> shaderIDs;
  for (auto& [shaderPath, shaderType, replace] : shaders)
  {
    // preprocess shader
    auto compileResult = spvPreprocessAndCompile(compiler,
      options,
      replace,
      shaderPath,
      gl2shadercTypes.at(shaderType));
    
    // cleanup existing shaders
    if (compileResult.size() == 0)
    {
      for (auto ID : shaderIDs)
        glDeleteShader(ID);
      break;
    }

    // "compile" (upload binary) shader
    GLuint shaderID = glCreateShader(shaderType);
    shaderIDs.push_back(shaderID);
    glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, compileResult.data(), static_cast<GLsizei>(compileResult.size() * sizeof(uint32_t)));
    glSpecializeShader(shaderID, "main", 0, 0, 0);

    // check if shader compilation succeeded
    GLint compileStatus = 0;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_FALSE)
    {
      const int LOG_BUF_LEN = 512;
      GLint maxlen = 0;
      GLchar infoLog[LOG_BUF_LEN];
      glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxlen);
      glGetShaderInfoLog(shaderID, LOG_BUF_LEN, NULL, infoLog);

      printf("File: %s\n", shaderPath.c_str());
      printf("Error binary-ing shader of type %d\n%s\n", shaderType, infoLog);
    }
  }

  programID = glCreateProgram();

  for (auto ID : shaderIDs)
    glAttachShader(programID, ID);

  glLinkProgram(programID);

  GLint success;
  GLchar infoLog[512];
  glGetProgramiv(programID, GL_LINK_STATUS, &success);
  if (!success)
  {
    glGetProgramInfoLog(programID, 512, NULL, infoLog);
    printf("Failed to link shader program.\nFiles:\n");
    for (auto [path, type, repl] : shaders)
      std::printf("%s\n", path.c_str());
    std::cout << "Failed to link shader program\n" << infoLog << std::endl;
  }

  std::vector<std::string_view> strs;
  for (const auto& [shaderPath, shaderType, replace] : shaders)
    strs.push_back(shaderPath);
  checkLinkStatus(strs);

  initUniforms();

  for (auto shaderID : shaderIDs)
    glDetachShader(programID, shaderID);
}


// loads a shader source into a string (string_view doesn't support concatenation)
std::string Shader::loadFile(std::string path)
{
  std::string shaderpath = path;
  std::string content;
  try
  {
    std::ifstream ifs(shaderpath);
    content = std::string((std::istreambuf_iterator<char>(ifs)),
      (std::istreambuf_iterator<char>()));
  }
  catch (std::ifstream::failure e)
  {
    std::cout << "Error reading shader file: " << path << '\n';
    std::cout << "Message: " << e.what() << std::endl;
  }
  return content;
}


// compiles a shader source and returns its ID
GLint Shader::compileShader(shaderType type, const std::vector<std::string>& src, std::string_view path)
{
  GLuint shader = 0;
  GLchar infoLog[512];
  GLint success;

  shader = glCreateShader(type);

  const GLchar** strings = new const GLchar*[src.size()];
  for (int i = 0; i < src.size(); i++)
  {
    strings[i] = src[i].data();
  }

  glShaderSource(shader, static_cast<GLsizei>(src.size()), strings, NULL);
  glCompileShader(shader);

  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);

    std::cout << "File: " << path << std::endl;
    std::cout << "Error compiling shader of type " << type << '\n' << infoLog << std::endl;
  }
  else
  {
    // compile successful
  }

  delete[] strings;
  return shader;
}


// TODO: https://github.com/fendevel/Guide-to-Modern-OpenGL-Functions#ideal-way-of-retrieving-all-uniform-names
void Shader::initUniforms()
{
  // init uniform map used in that shader
  GLint max_length;
  GLint num_uniforms;

  glGetProgramiv(programID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);
  GLchar* pname = new GLchar[max_length];
  glGetProgramiv(programID, GL_ACTIVE_UNIFORMS, &num_uniforms);

  for (GLint i = 0; i < num_uniforms; ++i)
  {
    GLsizei written;
    GLint size;
    GLenum type;
    
    glGetActiveUniform(programID, i, max_length, &written, &size, &type, pname);
    GLchar* pname1 = new GLchar[max_length];
    std::memcpy(pname1, pname, max_length * sizeof(GLchar));
    if (size > 1)
      pname1[written - 3] = '\0';
    GLint loc = glGetUniformLocation(programID, pname1);
    Uniforms.emplace(pname1, loc);
    //delete[] pname1;
  }

  delete[] pname;
}


void Shader::checkLinkStatus(std::vector<std::string_view> files)
{
  // link program
  GLint success;
  GLchar infoLog[512];
  glGetProgramiv(programID, GL_LINK_STATUS, &success);
  if (!success)
  {
    glGetProgramInfoLog(programID, 512, NULL, infoLog);
    std::cout << "File(s): ";// << vertexPath << ", " << fragmentPath << '\n';
    for (const auto& file : files)
      std::cout << file << (file == *(files.end() - 1) ? "" : ", "); // no comma on last element
    std::cout << '\n';
    std::cout << "Failed to link shader program\n" << infoLog << std::endl;
    //traceMessage(std::string(infoLog));
  }
  else
  {
    // link successful
  }
}



std::vector<uint32_t>
  Shader::spvPreprocessAndCompile(
    shaderc::Compiler& compiler,
    const shaderc::CompileOptions options,
    const std::vector<std::pair<std::string, std::string>>& replace,
    std::string path,
    shaderc_shader_kind shaderType)
{
  // vert/frag required
  std::string rawSrc = loadFile(path);
  for (const auto& [search, replacement] : replace)
  {
    rawSrc = std::regex_replace(rawSrc, std::regex(search), replacement);
  }

  auto PreprocessResult = compiler.PreprocessGlsl(
    rawSrc, shaderType, path.c_str(), options);
  if (auto numErr = PreprocessResult.GetNumErrors(); numErr > 0)
  {
    PreprocessResult.GetCompilationStatus();
    printf("%llu errors preprocessing %s!\n", numErr, path.c_str());
    printf("%s", PreprocessResult.GetErrorMessage().c_str());
    return {};
  }

  auto CompileResult = compiler.CompileGlslToSpv(
    PreprocessResult.begin(), shaderType, path.c_str(), options);
  if (auto numErr = CompileResult.GetNumErrors(); numErr > 0)
  {
    printf("%llu errors compiling %s!\n", numErr, path.c_str());
    printf("%s", CompileResult.GetErrorMessage().c_str());
    return {};
  }

  return { CompileResult.begin(), CompileResult.end() };
}