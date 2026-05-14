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

template<>
std::shared_ptr<Texture> ResourceManager::Load<Texture>(const std::string& path) {
    std::string key = MakeKey<Texture>(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Texture>(p);
    }
    auto tex = Texture::Load2D(path, true);
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
    auto mesh = MeshLoader::Load(path);
    cache_[key] = {mesh};
    return mesh;
}

} // namespace HuanGL
