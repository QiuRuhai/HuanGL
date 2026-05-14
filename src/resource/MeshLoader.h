#pragma once
#include <memory>
#include <string>
#include "../renderer/Schema.h"

namespace HuanGL {
class MeshLoader {
public:
    static std::shared_ptr<Mesh> Load(const std::string& path);
};
} // namespace HuanGL
