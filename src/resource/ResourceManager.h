#pragma once
#include <string>
#include <unordered_map>
#include <memory>

namespace HuanGL {

class ResourceManager {
public:
    static void Init();
    static void Shutdown();

    template<typename T>
    static std::shared_ptr<T> Load(const std::string& path);

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
