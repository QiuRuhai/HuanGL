#include "GpuProfiler.h"

namespace HuanGL {

GpuProfiler::~GpuProfiler() {
    for (auto& slot : frames_) {
        for (auto& s : slot.stages) {
            if (s.start) glDeleteQueries(1, &s.start);
            if (s.end)   glDeleteQueries(1, &s.end);
        }
    }
}

void GpuProfiler::BeginFrame() {
    current_ = &frames_[frameCount_ % kFrameDepth];
    // This slot, if pending, holds queries issued kFrameDepth frames ago.
    // Those timestamps are guaranteed ready, so this readback never stalls.
    if (current_->pending) {
        Resolve(*current_);
        current_->pending = false;
    }
    stageCursor_ = 0;
}

void GpuProfiler::BeginStage(const char* name) {
    if (!current_) return;
    if (stageCursor_ >= static_cast<int>(current_->stages.size())) {
        StageQuery q;
        glCreateQueries(GL_TIMESTAMP, 1, &q.start);
        glCreateQueries(GL_TIMESTAMP, 1, &q.end);
        current_->stages.push_back(q);
    }
    StageQuery& q = current_->stages[stageCursor_];
    q.name = name;
    glQueryCounter(q.start, GL_TIMESTAMP);
}

void GpuProfiler::EndStage() {
    if (!current_ || stageCursor_ >= static_cast<int>(current_->stages.size()))
        return;
    glQueryCounter(current_->stages[stageCursor_].end, GL_TIMESTAMP);
    ++stageCursor_;
}

void GpuProfiler::EndFrame() {
    if (!current_) return;
    current_->pending = (stageCursor_ > 0);
    current_ = nullptr;
    ++frameCount_;
}

void GpuProfiler::Resolve(FrameSlot& slot) {
    results_.clear();
    results_.reserve(slot.stages.size());
    for (auto& s : slot.stages) {
        GLuint64 start = 0, end = 0;
        glGetQueryObjectui64v(s.start, GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(s.end,   GL_QUERY_RESULT, &end);
        double ms = (end >= start) ? static_cast<double>(end - start) / 1.0e6
                                   : 0.0;
        results_.push_back({s.name, ms});
    }
}

} // namespace HuanGL
