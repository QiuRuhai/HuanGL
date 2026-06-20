#include "PathTracerStage.h"
#include "../PipelineResources.h"
#include "../../renderer/FrameContext.h"
#include "../../renderer/RenderSceneView.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>

namespace HuanGL {

PathTracerStage::PathTracerStage(std::string hdrPath)
    : hdrPath_(std::move(hdrPath))
{
}

void PathTracerStage::Init(int width, int height) {
    width_  = width;
    height_ = height;
    accum_  = Texture::Create2D(width_, height_, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    shader_ = std::make_unique<Shader>("pathtracer/pathtrace.comp");
    env_    = Texture::LoadHDR(hdrPath_); // used in Task 6
    sampleCount_ = 0;
    sceneDirty_  = true;
}

void PathTracerStage::Resize(int width, int height) {
    width_  = width;
    height_ = height;
    accum_  = Texture::Create2D(width_, height_, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    InvalidateHistory();
}

void PathTracerStage::InvalidateHistory() {
    // Scene switch / resize: rebuild BVH next Execute and clear accumulation.
    sampleCount_ = 0;
    sceneDirty_  = true;
}

void PathTracerStage::Execute(PipelineResources& resources, const FrameContext& frame) {
    if (!frame.renderSettings.pathTracerEnabled)
        return;

    // Camera-move detection: reset accumulation only (geometry unchanged).
    if (frame.camera.unjitteredViewProj != lastViewProj_) {
        sampleCount_  = 0;
        lastViewProj_ = frame.camera.unjitteredViewProj;
    }

    // Scene rebuild only when dirty (geometry changed or first frame).
    if (sceneDirty_) {
        scene_.Build(resources.Get<RenderSceneView>());
        sceneDirty_  = false;
        sampleCount_ = 0;
    }

    if (!scene_.Ready())
        return;

    // Bind accumulation buffer as read-write image.
    accum_->BindImage(0, GL_READ_WRITE, GL_RGBA32F);

    // Bind BVH / triangle / material SSBOs.
    scene_.BindSSBOs();

    // Reference camera uses the unjittered transform (no TAA jitter on ground truth).
    glm::mat4 invVP = glm::inverse(frame.camera.unjitteredViewProj);
    shader_->SetMat4("uInvViewProj", invVP);
    shader_->SetVec3("uCamPos",      frame.camera.camPos);
    shader_->SetVec2("uResolution",  glm::vec2(static_cast<float>(width_),
                                               static_cast<float>(height_)));
    shader_->SetInt("uSampleIndex",  static_cast<int>(sampleCount_));

    shader_->Dispatch((width_ + 7) / 8, (height_ + 7) / 8);

    ++sampleCount_;

    // Temporary verification hook — removed in Task 9.
    std::printf("[PT] spp=%u\n", sampleCount_);

    // Publish reference outputs; consumers divide hdr by sampleCount to get the mean.
    ReferenceOutputs out;
    out.hdr         = accum_;
    out.sampleCount = sampleCount_;
    resources.Set(out);
}

} // namespace HuanGL
