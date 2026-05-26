#pragma once

namespace HuanGL {

struct ApplicationState;

class InputController {
public:
    void Update(ApplicationState& state, float deltaTime);

private:
    bool wasCameraActive_ = false;
};

} // namespace HuanGL
