// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2020 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "Open3D/Visualization/Rendering/Filament/FilamentRenderToBuffer.h"

#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/View.h>
#include <filament/Viewport.h>

#include "Open3D/Utility/Console.h"
#include "Open3D/Visualization/Rendering/Filament/FilamentEngine.h"
#include "Open3D/Visualization/Rendering/Filament/FilamentRenderer.h"
#include "Open3D/Visualization/Rendering/Filament/FilamentScene.h"
#include "Open3D/Visualization/Rendering/Filament/FilamentView.h"

namespace open3d {
namespace visualization {

FilamentRenderToBuffer::FilamentRenderToBuffer(filament::Engine& engine)
    : engine_(engine) {
    renderer_ = engine_.createRenderer();
}

FilamentRenderToBuffer::FilamentRenderToBuffer(filament::Engine& engine,
                                               FilamentRenderer& parent)
    : parent_(&parent), engine_(engine) {
    renderer_ = engine_.createRenderer();
}

FilamentRenderToBuffer::~FilamentRenderToBuffer() {
    engine_.destroy(swapchain_);
    engine_.destroy(renderer_);

    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;

        buffer_size_ = 0;
    }

    if (parent_) {
        parent_->OnBufferRenderDestroyed(this);
        parent_ = nullptr;
    }
}

void FilamentRenderToBuffer::SetDimensions(const std::size_t width,
                                           const std::size_t height) {
    if (swapchain_) {
        engine_.destroy(swapchain_);
    }

    swapchain_ = engine_.createSwapChain(width, height,
                                         filament::SwapChain::CONFIG_READABLE);
    view_->SetViewport(0, 0, width, height);

    width_ = width;
    height_ = height;

    buffer_size_ = width * height * 3 * sizeof(std::uint8_t);
    if (buffer_) {
        buffer_ = static_cast<std::uint8_t*>(realloc(buffer_, buffer_size_));
    }
}

void FilamentRenderToBuffer::CopySettings(const View* view) {
    auto* downcast = dynamic_cast<const FilamentView*>(view);
    // NOTE: This class used to copy parameters from the view into a view
    // managed by this class. However, the copied view caused anomalies when
    // rendering an image for export. As a workaround, we keep a pointer to the
    // original view here instead.
    view_ = const_cast<FilamentView*>(downcast);
    if (downcast) {
        auto vp = view_->GetNativeView()->getViewport();
        SetDimensions(vp.width, vp.height);
    }
}

View& FilamentRenderToBuffer::GetView() { return *view_; }

using PBDParams = std::tuple<FilamentRenderToBuffer*,
                             FilamentRenderToBuffer::BufferReadyCallback>;
void FilamentRenderToBuffer::RequestFrame(Scene* scene,
                                          BufferReadyCallback callback) {
    if (!scene) {
        utility::LogDebug(
                "No Scene object was provided for rendering into buffer");
        callback({0, 0, nullptr, 0});
        return;
    }

    if (pending_) {
        utility::LogWarning(
                "Render to buffer can process only one request at time");
        callback({0, 0, nullptr, 0});
        return;
    }

    pending_ = true;

    if (buffer_ == nullptr) {
        buffer_ = static_cast<std::uint8_t*>(malloc(buffer_size_));
    }

    callback_ = callback;
}

void FilamentRenderToBuffer::ReadPixelsCallback(void*, size_t, void* user) {
    auto params = static_cast<PBDParams*>(user);
    FilamentRenderToBuffer* self;
    BufferReadyCallback callback;
    std::tie(self, callback) = *params;

    callback({self->width_, self->height_, self->buffer_, self->buffer_size_});

    self->frame_done_ = true;
    delete params;
}

void FilamentRenderToBuffer::Render() {
    bool shotDone = false;
    frame_done_ = false;
    while (!frame_done_) {
        if (renderer_->beginFrame(swapchain_)) {
            renderer_->render(view_->GetNativeView());

            if (!shotDone) {
                shotDone = true;

                using namespace filament;
                using namespace backend;

                auto user_param = new PBDParams(this, callback_);
                PixelBufferDescriptor pd(
                        buffer_, buffer_size_, PixelDataFormat::RGB,
                        PixelDataType::UBYTE, ReadPixelsCallback, user_param);

                auto vp = view_->GetNativeView()->getViewport();

                renderer_->readPixels(vp.left, vp.bottom, vp.width, vp.height,
                                      std::move(pd));
            }

            renderer_->endFrame();
        }
    }

    pending_ = false;
}

}  // namespace visualization
}  // namespace open3d
