// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2019 www.open3d.org
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

#include <filament/Color.h>
#include <memory>
#include <numeric>

#include "Open3D/Visualization/Rendering/View.h"

namespace filament {
class Engine;
class Scene;
class View;
class Viewport;
}  // namespace filament

namespace open3d {
namespace visualization {

class FilamentCamera;
class FilamentResourceManager;
class FilamentScene;

class FilamentView : public View {
public:
    static constexpr std::uint8_t kAllLayersMask =
            std::numeric_limits<std::uint8_t>::max();
    static constexpr std::uint8_t kMainLayer = 1;  // Default layer for objects

    FilamentView(filament::Engine& engine,
                 FilamentResourceManager& resource_mgr);
    FilamentView(filament::Engine& engine,
                 FilamentScene& scene,
                 FilamentResourceManager& resource_mgr);
    ~FilamentView() override;

    Mode GetMode() const override;
    void SetMode(Mode mode) override;
    void SetDiscardBuffers(const TargetBuffers& buffers) override;

    void SetSampleCount(int n) override;
    int GetSampleCount() const override;

    void SetViewport(std::int32_t x,
                     std::int32_t y,
                     std::uint32_t w,
                     std::uint32_t h) override;
    void SetClearColor(const Eigen::Vector3f& color) override;

    void SetSSAOEnabled(bool enabled) override;

    Camera* GetCamera() const override;

    // Copies available settings for view and camera
    void CopySettingsFrom(const FilamentView& other);

    void SetScene(FilamentScene& scene);

    filament::View* GetNativeView() const { return view_; }

    void PreRender();
    void PostRender();

private:
    std::unique_ptr<FilamentCamera> camera_;
    Eigen::Vector3f clear_color_;
    Mode mode_ = Mode::Color;
    TargetBuffers discard_buffers_;

    filament::Engine& engine_;
    FilamentScene* scene_ = nullptr;
    FilamentResourceManager& resource_mgr_;
    filament::View* view_ = nullptr;
};

}  // namespace visualization
}  // namespace open3d
