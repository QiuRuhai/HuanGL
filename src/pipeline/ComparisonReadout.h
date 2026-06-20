#pragma once
#include <cstdint>

namespace HuanGL {

struct ComparisonReadout {
    double   rmse        = 0.0;
    double   mape        = 0.0;
    uint32_t sampleCount = 0;
    bool     valid       = false;
};

} // namespace HuanGL
