#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>

namespace HuanGL {

struct StageTiming {
    std::string name;
    double ms = 0.0;
};

// Per-stage GPU timing using GL_TIMESTAMP query pairs, ring-buffered across
// kFrameDepth frames. We read back a slot only when it is kFrameDepth frames
// old, so the results are already available and glGetQueryObject never stalls
// the GPU. Assumes a stable stage list frame-to-frame (true for the fixed
// RenderPipeline); the query objects are reused every frame.
class GpuProfiler {
public:
    GpuProfiler() = default;
    ~GpuProfiler();
    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    void BeginFrame();
    void BeginStage(const char* name);
    void EndStage();
    void EndFrame();

    // Timings for the most recently resolved frame. Empty until the ring has
    // filled (first kFrameDepth frames).
    const std::vector<StageTiming>& GetResults() const { return results_; }

private:
    static constexpr unsigned int kFrameDepth = 3;

    struct StageQuery {
        std::string name;
        GLuint start = 0;
        GLuint end   = 0;
    };
    struct FrameSlot {
        std::vector<StageQuery> stages;
        bool pending = false;
    };

    void Resolve(const FrameSlot& slot);

    FrameSlot frames_[kFrameDepth];
    FrameSlot* current_ = nullptr;
    // Unsigned so the frameCount_ % kFrameDepth wrap is well-defined on
    // overflow (signed overflow would be UB after very long runtimes).
    unsigned int frameCount_ = 0;
    int stageCursor_ = 0;
    std::vector<StageTiming> results_;
};

} // namespace HuanGL
