#pragma once
#include <any>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace HuanGL {

class PipelineResources {
public:
    template<typename T>
    void Set(T resource) {
        resources_[std::type_index(typeid(T))] = std::move(resource);
    }

    template<typename T>
    const T& Get() const {
        auto it = resources_.find(std::type_index(typeid(T)));
        if (it == resources_.end())
            throw std::runtime_error("PipelineResources: missing resource");
        return std::any_cast<const T&>(it->second);
    }

    template<typename T>
    bool Has() const {
        return resources_.count(std::type_index(typeid(T))) > 0;
    }

    void Clear() { resources_.clear(); }

private:
    std::unordered_map<std::type_index, std::any> resources_;
};

} // namespace HuanGL
