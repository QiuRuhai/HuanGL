#pragma once
#include "Scene.h"

namespace HuanGL {

// A primitive Cornell box: a unit-ish room with colored side walls and two
// inner boxes, all constant-color (factor-only) diffuse materials. This is the
// canonical diffuse-GI validation scene -- color bleeding from the red/green
// walls onto the boxes is the headline result the reference path tracer must
// reproduce and the realtime renderer (no interreflection) must miss.
class CornellScene : public Scene {
public:
    void Init(ResourceManager& rm) override;
};

} // namespace HuanGL
