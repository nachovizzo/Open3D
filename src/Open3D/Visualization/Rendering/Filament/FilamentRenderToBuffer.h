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

#pragma once

#include "Open3D/Visualization/Rendering/RenderToBuffer.h"

namespace filament {
class Engine;
class Renderer;
class Scene;
class SwapChain;
}  // namespace filament

namespace open3d {
namespace visualization {

class FilamentRenderer;
class FilamentScene;
class FilamentView;

class FilamentRenderToBuffer : public RenderToBuffer {
public:
    // Use Renderer::CreateBufferRenderer to instantiate
    // FilamentRenderToBuffer, unless you are NOT using open3d::gui or
    // another FilamentRenderer instance.
    explicit FilamentRenderToBuffer(filament::Engine& engine);
    FilamentRenderToBuffer(filament::Engine& engine, FilamentRenderer& parent);
    ~FilamentRenderToBuffer() override;

    void SetDimensions(std::size_t width, std::size_t height) override;
    void CopySettings(const View* view) override;
    View& GetView() override;

    void RequestFrame(Scene* scene, BufferReadyCallback cb) override;

private:
    friend class FilamentRenderer;

    FilamentRenderer* parent_ = nullptr;
    filament::Engine& engine_;
    filament::Renderer* renderer_ = nullptr;
    filament::SwapChain* swapchain_ = nullptr;
    FilamentView* view_ = nullptr;

    std::size_t width_ = 0;
    std::size_t height_ = 0;
    std::uint8_t* buffer_ = nullptr;
    std::size_t buffer_size_ = 0;

    BufferReadyCallback callback_;
    bool frame_done_ = true;
    bool pending_ = false;

    static void ReadPixelsCallback(void* buffer, size_t size, void* user);
    void Render();
};

}  // namespace visualization
}  // namespace open3d
