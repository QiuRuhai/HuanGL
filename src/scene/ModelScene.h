#pragma once
#include "Scene.h"
#include <string>

namespace HuanGL {

/// Scene that loads a single glTF/OBJ/FBX model via MeshLoader and adds a
/// floor plane for shadow reception. Used to verify material/normal-map/IBL
/// pipeline end-to-end with real assets.
class ModelScene : public Scene {
public:
    /// @param modelPath absolute or relative-to-cwd path to the model file
    /// @param sceneName display name for logging
    /// @param addFloor whether to add a procedural floor plane (default true)
    /// @param modelScale uniform scale applied to the model
    explicit ModelScene(std::string modelPath,
                        std::string sceneName,
                        bool addFloor = true,
                        float modelScale = 1.0f);

    void Init(ResourceManager& rm) override;

    const std::string& GetName() const { return sceneName_; }

private:
    std::string modelPath_;
    std::string sceneName_;
    bool        addFloor_;
    float       modelScale_;
};

} // namespace HuanGL
