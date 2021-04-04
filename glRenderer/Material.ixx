module;

#include <unordered_map>
#include <optional>
#include <string>

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

  Material material;
  material.albedoTex = new Texture2D(albedoTexName, false, true);
  material.roughnessTex = new Texture2D(roughnessTexName, false, true);
  material.metalnessTex = new Texture2D(metalnessTexName, false, true);
  material.normalTex = new Texture2D(normalTexName, false, true);
  material.ambientOcclusionTex = new Texture2D(ambientOcclusionTexName, false, true);
  auto p = materials.insert({ name, material });
  return p.first->second;
}
