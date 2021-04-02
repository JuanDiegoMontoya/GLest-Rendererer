#include "Material.h"
#include "Texture.h"

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
  if (auto it  = materials.find(name); it != materials.end())
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
