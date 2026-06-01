#include "ResourceManager.h"
#include "MeshLoader.h"
#include "../renderer/Texture.h"

namespace HuanGL {

std::unordered_map<std::string, ResourceManager::Entry> ResourceManager::cache_;

void ResourceManager::Init() {}
void ResourceManager::Shutdown() { GC(); }

void ResourceManager::GC() {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->second.ptr.expired()) it = cache_.erase(it);
        else ++it;
    }
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& path,
                                                      bool sRGB) {
    std::string key = "Texture|" + path + (sRGB ? "|srgb" : "|linear");
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Texture>(p);
    }
    auto tex = Texture::Load2D(path, sRGB);
    cache_[key] = {tex};
    return tex;
}

template<>
std::shared_ptr<Mesh> ResourceManager::Load<Mesh>(const std::string& path) {
    std::string key = MakeKey<Mesh>(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Mesh>(p);
    }
    auto result = MeshLoader::Load(path);
    cache_[key] = {result.mesh};
    return result.mesh;
}

} // namespace HuanGL
