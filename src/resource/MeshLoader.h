#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../renderer/Schema.h"

namespace HuanGL {

struct LoadResult {
    std::shared_ptr<Mesh>   mesh;
    std::vector<Material>   materials;
};

class MeshLoader {
public:
    /// Load mesh + materials from file (glTF, OBJ, FBX, etc.)
    static LoadResult Load(const std::string& path);
};

} // namespace HuanGL
