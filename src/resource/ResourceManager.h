#pragma once
#include <string>
#include <unordered_map>
#include <memory>

namespace HuanGL {

class Texture;

class ResourceManager {
public:
    static void Init();
    static void Shutdown();

    // Mesh cache (keyed by path).
    template<typename T>
    static std::shared_ptr<T> Load(const std::string& path);

    // Texture cache keyed by (path, sRGB) so the same file can be cached both
    // as color (sRGB) and as linear data (normal/roughness/metallic) without
    // collisions, and shared linear textures are loaded only once.
    static std::shared_ptr<Texture> LoadTexture(const std::string& path, bool sRGB);

    static void GC();

private:
    template<typename T>
    static std::string MakeKey(const std::string& path) {
        return std::string(typeid(T).name()) + "|" + path;
    }
    struct Entry { std::weak_ptr<void> ptr; };
    static std::unordered_map<std::string, Entry> cache_;
};

} // namespace HuanGL
