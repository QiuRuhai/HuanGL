#pragma once

namespace HuanGL {

struct ApplicationState;

class InputController {
public:
    void Update(ApplicationState& state);

private:
    bool wasCameraActive_ = false;
};

} // namespace HuanGL
