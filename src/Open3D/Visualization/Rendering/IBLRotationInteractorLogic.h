// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
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

#include "Open3D/Visualization/Rendering/MatrixInteractorLogic.h"

#include "Open3D/Visualization/Rendering/RendererHandle.h"

namespace open3d {
namespace visualization {

class Scene;

class IBLRotationInteractorLogic : public MatrixInteractorLogic {
    using Super = MatrixInteractorLogic;

public:
    IBLRotationInteractorLogic(Scene* scene, Camera* camera);

    void Rotate(int dx, int dy) override;
    void RotateZ(int dx, int dy) override;

    void SetSkyboxHandle(visualization::SkyboxHandle skybox, bool is_on);

    void StartMouseDrag();
    void UpdateMouseDragUI();
    void EndMouseDrag();

    Camera::Transform GetCurrentRotation() const;

private:
    Scene* scene_;
    Camera* camera_;
    visualization::SkyboxHandle skybox_;
    bool skybox_is_normally_on_;
    Camera::Transform ibl_rotation_at_mouse_down_;

    struct UIObj {
        GeometryHandle handle;
        Camera::Transform transform;
    };
    std::vector<UIObj> ui_objs_;

    void ClearUI();
};

}  // namespace visualization
}  // namespace open3d
