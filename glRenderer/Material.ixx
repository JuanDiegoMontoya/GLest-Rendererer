module;

#include <unordered_map>
#include <optional>
#include <string>
#include <glad/glad.h>

export module Material;

import GPU.Texture;

export struct Material
{
  Texture2D* albedoTex{};
  Texture2D* roughnessTex{};
  Texture2D* metalnessTex{};
  Texture2D* normalTex{};
  Texture2D* ambientOcclusionTex{};
};

// sent to GPU
export struct BindlessMaterial
{
  uint64_t albedoHandle{};
  uint64_t roughnessHandle{};
  uint64_t metalnessHandle{};
  uint64_t normalHandle{};
  uint64_t ambientOcclusionHandle{};
};

export class MaterialManager
{
public:
  MaterialManager() {}
  ~MaterialManager();
  std::optional<Material> GetMaterial(const std::string& mat);
  Material& MakeMaterial(std::string name,
    std::string albedoTexName,
    std::string roughnessTexName,
    std::string metalnessTexName,
    std::string normalTexName,
    std::string ambientOcclusionTexName);
  std::vector<std::pair<std::string, Material>> GetLinearMaterials()
  {
    return { materials.begin(), materials.end() };
  }

private:
  std::unordered_map<std::string, Material> materials;
};

MaterialManager::~MaterialManager()
{
  for (auto& p : materials)
  {
    delete p.second.albedoTex;
    delete p.second.roughnessTex;
    delete p.second.metalnessTex;
    delete p.second.normalTex;
    delete p.second.ambientOcclusionTex;
  }
}

std::optional<Material> MaterialManager::GetMaterial(const std::string& mat)
{
  auto it = materials.find(mat);
  if (it == materials.end())
  {
    return std::optional<Material>();
  }
  return it->second;
}

Material& MaterialManager::MakeMaterial(std::string name,
  std::string albedoTexName,
  std::string roughnessTexName,
  std::string metalnessTexName,
  std::string normalTexName,
  std::string ambientOcclusionTexName)
{
  if (auto it = materials.find(name); it != materials.end())
  {
    return it->second;
  }

  TextureCreateInfo info;
  info.generateMips = true;
  info.HDR = false;
  info.minFilter = GL_LINEAR_MIPMAP_LINEAR;
  info.magFilter = GL_LINEAR;
  info.sRGB = false;

  Material material;
  info.path = albedoTexName;
  material.albedoTex = new Texture2D(info);
  info.path = roughnessTexName;
  material.roughnessTex = new Texture2D(info);
  info.path = metalnessTexName;
  material.metalnessTex = new Texture2D(info);
  info.path = normalTexName;
  material.normalTex = new Texture2D(info);
  info.path = ambientOcclusionTexName;
  material.ambientOcclusionTex = new Texture2D(info);
  auto p = materials.insert({ name, material });
  return p.first->second;
}
