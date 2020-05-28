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

#include "Open3D/GUI/SceneWidget.h"

#include <Eigen/Geometry>
#include <set>

#include "Open3D/GUI/Application.h"
#include "Open3D/GUI/Color.h"
#include "Open3D/GUI/Events.h"
#include "Open3D/Geometry/BoundingVolume.h"
#include "Open3D/Visualization/Rendering/Camera.h"
#include "Open3D/Visualization/Rendering/CameraInteractorLogic.h"
#include "Open3D/Visualization/Rendering/IBLRotationInteractorLogic.h"
#include "Open3D/Visualization/Rendering/LightDirectionInteractorLogic.h"
#include "Open3D/Visualization/Rendering/ModelInteractorLogic.h"
#include "Open3D/Visualization/Rendering/Scene.h"
#include "Open3D/Visualization/Rendering/View.h"

namespace open3d {
namespace gui {

static const double NEAR_PLANE = 0.1;
static const double MIN_FAR_PLANE = 1.0;

static const double DELAY_FOR_BEST_RENDERING_SECS = 0.2;  // seconds
// ----------------------------------------------------------------------------
class MouseInteractor {
public:
    virtual ~MouseInteractor() = default;

    virtual visualization::MatrixInteractorLogic& GetMatrixInteractor() = 0;
    virtual void Mouse(const MouseEvent& e) = 0;
    virtual void Key(const KeyEvent& e) = 0;
    virtual bool Tick(const TickEvent& e) { return false; }
};

class RotateSunInteractor : public MouseInteractor {
public:
    RotateSunInteractor(visualization::Scene* scene,
                        visualization::Camera* camera)
        : light_dir_(std::make_unique<
                     visualization::LightDirectionInteractorLogic>(scene,
                                                                   camera)) {}

    visualization::MatrixInteractorLogic& GetMatrixInteractor() override {
        return *light_dir_.get();
    }

    void SetDirectionalLight(
            visualization::LightHandle dir_light,
            std::function<void(const Eigen::Vector3f&)> on_changed) {
        light_dir_->SetDirectionalLight(dir_light);
        on_light_dir_changed_ = on_changed;
    }

    void Mouse(const MouseEvent& e) override {
        switch (e.type) {
            case MouseEvent::BUTTON_DOWN:
                mouse_down_x_ = e.x;
                mouse_down_y_ = e.y;
                light_dir_->StartMouseDrag();
                break;
            case MouseEvent::DRAG: {
                int dx = e.x - mouse_down_x_;
                int dy = e.y - mouse_down_y_;
                light_dir_->Rotate(dx, dy);
                if (on_light_dir_changed_) {
                    on_light_dir_changed_(light_dir_->GetCurrentDirection());
                }
                break;
            }
            case MouseEvent::WHEEL: {
                break;
            }
            case MouseEvent::BUTTON_UP:
                light_dir_->EndMouseDrag();
                break;
            default:
                break;
        }
    }

    void Key(const KeyEvent& e) override {}

private:
    std::unique_ptr<visualization::LightDirectionInteractorLogic> light_dir_;
    int mouse_down_x_ = 0;
    int mouse_down_y_ = 0;
    std::function<void(const Eigen::Vector3f&)> on_light_dir_changed_;
};

class RotateIBLInteractor : public MouseInteractor {
public:
    RotateIBLInteractor(visualization::Scene* scene,
                        visualization::Camera* camera)
        : ibl_(std::make_unique<visualization::IBLRotationInteractorLogic>(
                  scene, camera)) {}

    visualization::MatrixInteractorLogic& GetMatrixInteractor() override {
        return *ibl_.get();
    }

    void SetSkyboxHandle(visualization::SkyboxHandle skybox, bool is_on) {
        ibl_->SetSkyboxHandle(skybox, is_on);
    }

    void SetOnChanged(
            std::function<void(const visualization::Camera::Transform&)>
                    on_changed) {
        on_rotation_changed_ = on_changed;
    }

    void Mouse(const MouseEvent& e) override {
        switch (e.type) {
            case MouseEvent::BUTTON_DOWN:
                mouse_down_x_ = e.x;
                mouse_down_y_ = e.y;
                ibl_->StartMouseDrag();
                break;
            case MouseEvent::DRAG: {
                int dx = e.x - mouse_down_x_;
                int dy = e.y - mouse_down_y_;
                if (e.modifiers & int(KeyModifier::META)) {
                    ibl_->RotateZ(dx, dy);
                } else {
                    ibl_->Rotate(dx, dy);
                }
                if (on_rotation_changed_) {
                    on_rotation_changed_(ibl_->GetCurrentRotation());
                }
                break;
            }
            case MouseEvent::WHEEL: {
                break;
            }
            case MouseEvent::BUTTON_UP:
                ibl_->EndMouseDrag();
                break;
            default:
                break;
        }
    }

    void Key(const KeyEvent& e) override {}

private:
    std::unique_ptr<visualization::IBLRotationInteractorLogic> ibl_;
    int mouse_down_x_ = 0;
    int mouse_down_y_ = 0;
    std::function<void(const visualization::Camera::Transform&)>
            on_rotation_changed_;
};

class FlyInteractor : public MouseInteractor {
public:
    explicit FlyInteractor(visualization::Camera* camera)
        : camera_controls_(
                  std::make_unique<visualization::CameraInteractorLogic>(
                          camera, MIN_FAR_PLANE)) {}

    visualization::MatrixInteractorLogic& GetMatrixInteractor() override {
        return *camera_controls_.get();
    }

    void Mouse(const MouseEvent& e) override {
        switch (e.type) {
            case MouseEvent::BUTTON_DOWN:
                last_mouse_x_ = e.x;
                last_mouse_y_ = e.y;
                camera_controls_->StartMouseDrag();
                break;
            case MouseEvent::DRAG: {
                // Use relative movement because user may be moving
                // with keys at the same time.
                int dx = e.x - last_mouse_x_;
                int dy = e.y - last_mouse_y_;
                if (e.modifiers & int(KeyModifier::META)) {
                    // RotateZ() was not intended to be used for relative
                    // movement, so reset the mouse-down matrix first.
                    camera_controls_->ResetMouseDrag();
                    camera_controls_->RotateZ(dx, dy);
                } else {
                    camera_controls_->RotateFly(-dx, -dy);
                }
                last_mouse_x_ = e.x;
                last_mouse_y_ = e.y;
                break;
            }
            case MouseEvent::WHEEL: {
                break;
            }
            case MouseEvent::BUTTON_UP:
                camera_controls_->EndMouseDrag();
                break;
            default:
                break;
        }
    }

    void Key(const KeyEvent& e) override {
        switch (e.type) {
            case KeyEvent::Type::DOWN:
                keys_down_.insert(e.key);
                break;
            case KeyEvent::Type::UP:
                keys_down_.erase(e.key);
                break;
        }
    }

    bool Tick(const TickEvent& e) override {
        bool redraw = false;
        if (!keys_down_.empty()) {
            auto& bounds = camera_controls_->GetBoundingBox();
            const float dist = 0.0025f * bounds.GetExtent().norm();
            const float angle_rad = 0.0075f;

            auto HasKey = [this](uint32_t key) -> bool {
                return (keys_down_.find(key) != keys_down_.end());
            };

            auto move = [this, &redraw](const Eigen::Vector3f& v) {
                camera_controls_->MoveLocal(v);
                redraw = true;
            };
            auto rotate = [this, &redraw](float angle_rad,
                                          const Eigen::Vector3f& axis) {
                camera_controls_->RotateLocal(angle_rad, axis);
                redraw = true;
            };
            auto rotateZ = [this, &redraw](int dy) {
                camera_controls_->StartMouseDrag();
                camera_controls_->RotateZ(0, dy);
                redraw = true;
            };

            if (HasKey('a')) {
                move({-dist, 0, 0});
            }
            if (HasKey('d')) {
                move({dist, 0, 0});
            }
            if (HasKey('w')) {
                move({0, 0, -dist});
            }
            if (HasKey('s')) {
                move({0, 0, dist});
            }
            if (HasKey('q')) {
                move({0, dist, 0});
            }
            if (HasKey('z')) {
                move({0, -dist, 0});
            }
            if (HasKey('e')) {
                rotateZ(-2);
            }
            if (HasKey('r')) {
                rotateZ(2);
            }
            if (HasKey(KEY_UP)) {
                rotate(angle_rad, {1, 0, 0});
            }
            if (HasKey(KEY_DOWN)) {
                rotate(-angle_rad, {1, 0, 0});
            }
            if (HasKey(KEY_LEFT)) {
                rotate(angle_rad, {0, 1, 0});
            }
            if (HasKey(KEY_RIGHT)) {
                rotate(-angle_rad, {0, 1, 0});
            }
        }
        return redraw;
    }

private:
    std::unique_ptr<visualization::CameraInteractorLogic> camera_controls_;
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
    std::set<uint32_t> keys_down_;
};

class RotationInteractor : public MouseInteractor {
protected:
    void SetInteractor(visualization::RotationInteractorLogic* r) {
        interactor_ = r;
    }

public:
    visualization::MatrixInteractorLogic& GetMatrixInteractor() override {
        return *interactor_;
    }

    void SetCenterOfRotation(const Eigen::Vector3f& center) {
        interactor_->SetCenterOfRotation(center);
    }

    void Mouse(const MouseEvent& e) override {
        switch (e.type) {
            case MouseEvent::BUTTON_DOWN:
                mouse_down_x_ = e.x;
                mouse_down_y_ = e.y;
                if (e.button.button == MouseButton::LEFT) {
                    if (e.modifiers & int(KeyModifier::SHIFT)) {
#ifdef __APPLE__
                        if (e.modifiers & int(KeyModifier::ALT)) {
#else
                        if (e.modifiers & int(KeyModifier::CTRL)) {
#endif  // __APPLE__
                            state_ = State::ROTATE_Z;
                        } else {
                            state_ = State::DOLLY;
                        }
                    } else if (e.modifiers & int(KeyModifier::CTRL)) {
                        state_ = State::PAN;
                    } else if (e.modifiers & int(KeyModifier::META)) {
                        state_ = State::ROTATE_Z;
                    } else {
                        state_ = State::ROTATE_XY;
                    }
                } else if (e.button.button == MouseButton::RIGHT) {
                    state_ = State::PAN;
                }
                interactor_->StartMouseDrag();
                break;
            case MouseEvent::DRAG: {
                int dx = e.x - mouse_down_x_;
                int dy = e.y - mouse_down_y_;
                switch (state_) {
                    case State::NONE:
                        break;
                    case State::PAN:
                        interactor_->Pan(dx, dy);
                        break;
                    case State::DOLLY:
                        interactor_->Dolly(
                                dy, visualization::MatrixInteractorLogic::
                                            DragType::MOUSE);
                        break;
                    case State::ROTATE_XY:
                        interactor_->Rotate(dx, dy);
                        break;
                    case State::ROTATE_Z:
                        interactor_->RotateZ(dx, dy);
                        break;
                }
                interactor_->UpdateMouseDragUI();
                break;
            }
            case MouseEvent::WHEEL: {
                interactor_->Dolly(
                        2.0 * e.wheel.dy,
                        e.wheel.isTrackpad
                                ? visualization::MatrixInteractorLogic::
                                          DragType::TWO_FINGER
                                : visualization::MatrixInteractorLogic::
                                          DragType::WHEEL);
                break;
            }
            case MouseEvent::BUTTON_UP:
                interactor_->EndMouseDrag();
                state_ = State::NONE;
                break;
            default:
                break;
        }
    }

    void Key(const KeyEvent& e) override {}

protected:
    visualization::RotationInteractorLogic* interactor_ = nullptr;
    int mouse_down_x_ = 0;
    int mouse_down_y_ = 0;

    enum class State { NONE, PAN, DOLLY, ROTATE_XY, ROTATE_Z };
    State state_ = State::NONE;
};

class RotateModelInteractor : public RotationInteractor {
public:
    explicit RotateModelInteractor(visualization::Scene* scene,
                                   visualization::Camera* camera)
        : RotationInteractor(),
          rotation_(new visualization::ModelInteractorLogic(
                  scene, camera, MIN_FAR_PLANE)) {
        SetInteractor(rotation_.get());
    }

    void SetModel(visualization::GeometryHandle axes,
                  const std::vector<visualization::GeometryHandle>& objects) {
        rotation_->SetModel(axes, objects);
    }

private:
    std::unique_ptr<visualization::ModelInteractorLogic> rotation_;
    visualization::GeometryHandle axes_;
};

class RotateCameraInteractor : public RotationInteractor {
    using Super = RotationInteractor;

public:
    explicit RotateCameraInteractor(visualization::Camera* camera)
        : camera_controls_(
                  std::make_unique<visualization::CameraInteractorLogic>(
                          camera, MIN_FAR_PLANE)) {
        SetInteractor(camera_controls_.get());
    }

    void Mouse(const MouseEvent& e) override {
        switch (e.type) {
            case MouseEvent::BUTTON_DOWN:
            case MouseEvent::DRAG:
            case MouseEvent::BUTTON_UP:
            default:
                Super::Mouse(e);
                break;
            case MouseEvent::WHEEL: {
                if (e.modifiers == int(KeyModifier::SHIFT)) {
                    camera_controls_->Zoom(
                            e.wheel.dy,
                            e.wheel.isTrackpad
                                    ? visualization::MatrixInteractorLogic::
                                              DragType::TWO_FINGER
                                    : visualization::MatrixInteractorLogic::
                                              DragType::WHEEL);
                } else {
                    Super::Mouse(e);
                }
                break;
            }
        }
    }

private:
    std::unique_ptr<visualization::CameraInteractorLogic> camera_controls_;
};

// ----------------------------------------------------------------------------
class Interactors {
public:
    Interactors(visualization::Scene* scene, visualization::Camera* camera)
        : rotate_(std::make_unique<RotateCameraInteractor>(camera)),
          fly_(std::make_unique<FlyInteractor>(camera)),
          sun_(std::make_unique<RotateSunInteractor>(scene, camera)),
          ibl_(std::make_unique<RotateIBLInteractor>(scene, camera)),
          model_(std::make_unique<RotateModelInteractor>(scene, camera)) {
        current_ = rotate_.get();
    }

    void SetViewSize(const Size& size) {
        rotate_->GetMatrixInteractor().SetViewSize(size.width, size.height);
        fly_->GetMatrixInteractor().SetViewSize(size.width, size.height);
        sun_->GetMatrixInteractor().SetViewSize(size.width, size.height);
        ibl_->GetMatrixInteractor().SetViewSize(size.width, size.height);
        model_->GetMatrixInteractor().SetViewSize(size.width, size.height);
    }

    void SetBoundingBox(const geometry::AxisAlignedBoundingBox& bounds) {
        rotate_->GetMatrixInteractor().SetBoundingBox(bounds);
        fly_->GetMatrixInteractor().SetBoundingBox(bounds);
        sun_->GetMatrixInteractor().SetBoundingBox(bounds);
        ibl_->GetMatrixInteractor().SetBoundingBox(bounds);
        model_->GetMatrixInteractor().SetBoundingBox(bounds);
    }

    void SetCenterOfRotation(const Eigen::Vector3f& center) {
        rotate_->SetCenterOfRotation(center);
    }

    void SetDirectionalLight(
            visualization::LightHandle dirLight,
            std::function<void(const Eigen::Vector3f&)> onChanged) {
        sun_->SetDirectionalLight(dirLight, onChanged);
    }

    void SetSkyboxHandle(visualization::SkyboxHandle skybox, bool isOn) {
        ibl_->SetSkyboxHandle(skybox, isOn);
    }

    void SetModel(visualization::GeometryHandle axes,
                  const std::vector<visualization::GeometryHandle>& objects) {
        model_->SetModel(axes, objects);
    }

    SceneWidget::Controls GetControls() const {
        if (current_ == fly_.get()) {
            return SceneWidget::Controls::FLY;
        } else if (current_ == sun_.get()) {
            return SceneWidget::Controls::ROTATE_SUN;
        } else if (current_ == ibl_.get()) {
            return SceneWidget::Controls::ROTATE_IBL;
        } else if (current_ == model_.get()) {
            return SceneWidget::Controls::ROTATE_MODEL;
        } else {
            return SceneWidget::Controls::ROTATE_OBJ;
        }
    }

    void SetControls(SceneWidget::Controls mode) {
        switch (mode) {
            case SceneWidget::Controls::ROTATE_OBJ:
                current_ = rotate_.get();
                break;
            case SceneWidget::Controls::FLY:
                current_ = fly_.get();
                break;
            case SceneWidget::Controls::ROTATE_SUN:
                current_ = sun_.get();
                break;
            case SceneWidget::Controls::ROTATE_IBL:
                current_ = ibl_.get();
                break;
            case SceneWidget::Controls::ROTATE_MODEL:
                current_ = model_.get();
                break;
        }
    }

    void Mouse(const MouseEvent& e) {
        if (current_ == rotate_.get()) {
            if (e.type == MouseEvent::Type::BUTTON_DOWN &&
                (e.button.button == MouseButton::MIDDLE ||
                 e.modifiers == int(KeyModifier::ALT))) {
                override_ = sun_.get();
            }
        }

        if (override_) {
            override_->Mouse(e);
        } else if (current_) {
            current_->Mouse(e);
        }

        if (override_ && e.type == MouseEvent::Type::BUTTON_UP) {
            override_ = nullptr;
        }
    }

    void Key(const KeyEvent& e) {
        if (current_) {
            current_->Key(e);
        }
    }

    Widget::DrawResult Tick(const TickEvent& e) {
        if (current_) {
            if (current_->Tick(e)) {
                return Widget::DrawResult::REDRAW;
            }
        }
        return Widget::DrawResult::NONE;
    }

private:
    std::unique_ptr<RotateCameraInteractor> rotate_;
    std::unique_ptr<FlyInteractor> fly_;
    std::unique_ptr<RotateSunInteractor> sun_;
    std::unique_ptr<RotateIBLInteractor> ibl_;
    std::unique_ptr<RotateModelInteractor> model_;

    MouseInteractor* current_ = nullptr;
    MouseInteractor* override_ = nullptr;
};

// ----------------------------------------------------------------------------
struct SceneWidget::Impl {
    visualization::Scene& scene_;
    visualization::ViewHandle view_id_;
    visualization::Camera* camera_;
    geometry::AxisAlignedBoundingBox bounds_;
    std::shared_ptr<Interactors> controls_;
    ModelDescription model_;
    visualization::LightHandle dir_light_;
    std::function<void(const Eigen::Vector3f&)> on_light_dir_changed_;
    std::function<void(visualization::Camera*)> on_camera_changed_;
    int buttons_down_ = 0;
    double last_fast_time_ = 0.0;
    bool frame_rect_changed_ = false;

    explicit Impl(visualization::Scene& scene) : scene_(scene) {}
};

SceneWidget::SceneWidget(visualization::Scene& scene) : impl_(new Impl(scene)) {
    impl_->view_id_ = scene.AddView(0, 0, 1, 1);

    auto view = impl_->scene_.GetView(impl_->view_id_);
    impl_->camera_ = view->GetCamera();
    impl_->controls_ = std::make_shared<Interactors>(&scene, view->GetCamera());
}

SceneWidget::~SceneWidget() { impl_->scene_.RemoveView(impl_->view_id_); }

void SceneWidget::SetFrame(const Rect& f) {
    Super::SetFrame(f);

    impl_->controls_->SetViewSize(Size(f.width, f.height));

    // We need to update the viewport and camera, but we can't do it here
    // because we need to know the window height to convert the frame
    // to OpenGL coordinates. We will actually do the updating in Draw().
    impl_->frame_rect_changed_ = true;
}

void SceneWidget::SetBackgroundColor(const Color& color) {
    auto view = impl_->scene_.GetView(impl_->view_id_);
    view->SetClearColor({color.GetRed(), color.GetGreen(), color.GetBlue()});
}

void SceneWidget::SetDiscardBuffers(
        const visualization::View::TargetBuffers& buffers) {
    auto view = impl_->scene_.GetView(impl_->view_id_);
    view->SetDiscardBuffers(buffers);
}

void SceneWidget::SetupCamera(
        float verticalFoV,
        const geometry::AxisAlignedBoundingBox& geometry_bounds,
        const Eigen::Vector3f& center_of_rotation) {
    impl_->bounds_ = geometry_bounds;
    impl_->controls_->SetBoundingBox(geometry_bounds);

    GoToCameraPreset(CameraPreset::PLUS_Z);  // default OpenGL view

    auto f = GetFrame();
    float aspect = 1.0f;
    if (f.height > 0) {
        aspect = float(f.width) / float(f.height);
    }
    // The far plane needs to be the max absolute distance, not just the
    // max extent, so that axes are visible if requested.
    // See also RotationInteractorLogic::UpdateCameraFarPlane().
    auto far1 = impl_->bounds_.GetMinBound().norm();
    auto far2 = impl_->bounds_.GetMaxBound().norm();
    auto far3 =
            GetCamera()->GetModelMatrix().translation().cast<double>().norm();
    auto model_size = 2.0 * impl_->bounds_.GetExtent().norm();
    auto far = std::max(MIN_FAR_PLANE,
                        std::max(std::max(far1, far2), far3) + model_size);
    GetCamera()->SetProjection(verticalFoV, aspect, NEAR_PLANE, far,
                               visualization::Camera::FovType::Vertical);
}

void SceneWidget::SetCameraChangedCallback(
        std::function<void(visualization::Camera*)> on_cam_changed) {
    impl_->on_camera_changed_ = on_cam_changed;
}

void SceneWidget::SelectDirectionalLight(
        visualization::LightHandle dir_light,
        std::function<void(const Eigen::Vector3f&)> on_dir_changed) {
    impl_->dir_light_ = dir_light;
    impl_->on_light_dir_changed_ = on_dir_changed;
    impl_->controls_->SetDirectionalLight(
            dir_light, [this, dir_light](const Eigen::Vector3f& dir) {
                impl_->scene_.SetLightDirection(dir_light, dir);
                if (impl_->on_light_dir_changed_) {
                    impl_->on_light_dir_changed_(dir);
                }
            });
}

void SceneWidget::SetSkyboxHandle(visualization::SkyboxHandle skybox,
                                  bool is_on) {
    impl_->controls_->SetSkyboxHandle(skybox, is_on);
}

void SceneWidget::SetModel(const ModelDescription& desc) {
    impl_->model_ = desc;
    for (auto p : desc.fast_point_clouds) {
        impl_->scene_.SetEntityEnabled(p, false);
    }

    std::vector<visualization::GeometryHandle> objects;
    objects.reserve(desc.point_clouds.size() + desc.meshes.size());
    for (auto p : desc.point_clouds) {
        objects.push_back(p);
    }
    for (auto m : desc.meshes) {
        objects.push_back(m);
    }
    for (auto p : desc.fast_point_clouds) {
        objects.push_back(p);
    }
    impl_->controls_->SetModel(desc.axes, objects);
}

void SceneWidget::SetViewControls(Controls mode) {
    if (mode == Controls::ROTATE_OBJ &&
        impl_->controls_->GetControls() == Controls::FLY) {
        impl_->controls_->SetControls(mode);
        // If we're going from fly to standard rotate obj, we need to
        // adjust the center of rotation or it will jump to a different
        // matrix rather abruptly. The center of rotation is used for the
        // panning distance so that the cursor stays in roughly the same
        // position as the user moves the mouse. Use the distance to the
        // center of the model, which should be reasonable.
        Eigen::Vector3f to_center = impl_->bounds_.GetCenter().cast<float>() -
                                    impl_->camera_->GetPosition();
        Eigen::Vector3f forward = impl_->camera_->GetForwardVector();
        Eigen::Vector3f center =
                impl_->camera_->GetPosition() + to_center.norm() * forward;
        impl_->controls_->SetCenterOfRotation(center);
    } else {
        impl_->controls_->SetControls(mode);
    }
}

void SceneWidget::SetRenderQuality(Quality quality) {
    auto currentQuality = GetRenderQuality();
    if (currentQuality != quality) {
        bool is_fast = false;
        auto view = impl_->scene_.GetView(impl_->view_id_);
        if (quality == Quality::FAST) {
            view->SetSampleCount(1);
            is_fast = true;
        } else {
            view->SetSampleCount(4);
            is_fast = false;
        }
        if (!impl_->model_.fast_point_clouds.empty()) {
            for (auto p : impl_->model_.point_clouds) {
                impl_->scene_.SetEntityEnabled(p, !is_fast);
            }
            for (auto p : impl_->model_.fast_point_clouds) {
                impl_->scene_.SetEntityEnabled(p, is_fast);
            }
        }
    }
}

SceneWidget::Quality SceneWidget::GetRenderQuality() const {
    int n = impl_->scene_.GetView(impl_->view_id_)->GetSampleCount();
    if (n == 1) {
        return Quality::FAST;
    } else {
        return Quality::BEST;
    }
}

void SceneWidget::GoToCameraPreset(CameraPreset preset) {
    // To get the eye position we move maxDim away from the center in the
    // appropriate direction. We cannot simply use maxDim as that value
    // for that dimension, because the model may not be centered around
    // (0, 0, 0), and this will result in the far plane being not being
    // far enough and clipping the model. To test, use
    // https://docs.google.com/uc?export=download&id=0B-ePgl6HF260ODdvT09Xc1JxOFE
    float max_dim = 1.25f * impl_->bounds_.GetMaxExtent();
    Eigen::Vector3f center = impl_->bounds_.GetCenter().cast<float>();
    Eigen::Vector3f eye, up;
    switch (preset) {
        case CameraPreset::PLUS_X: {
            eye = Eigen::Vector3f(center.x() + max_dim, center.y(), center.z());
            up = Eigen::Vector3f(0, 1, 0);
            break;
        }
        case CameraPreset::PLUS_Y: {
            eye = Eigen::Vector3f(center.x(), center.y() + max_dim, center.z());
            up = Eigen::Vector3f(1, 0, 0);
            break;
        }
        case CameraPreset::PLUS_Z: {
            eye = Eigen::Vector3f(center.x(), center.y(), center.z() + max_dim);
            up = Eigen::Vector3f(0, 1, 0);
            break;
        }
    }
    impl_->camera_->LookAt(center, eye, up);
    impl_->controls_->SetCenterOfRotation(center);
}

visualization::View* SceneWidget::GetView() const {
    return impl_->scene_.GetView(impl_->view_id_);
}

visualization::Scene* SceneWidget::GetScene() const { return &impl_->scene_; }

visualization::Camera* SceneWidget::GetCamera() const {
    auto view = impl_->scene_.GetView(impl_->view_id_);
    return view->GetCamera();
}

Widget::DrawResult SceneWidget::Draw(const DrawContext& context) {
    // If the widget has changed size we need to update the viewport and the
    // camera. We can't do it in SetFrame() because we need to know the height
    // of the window to convert to OpenGL coordinates for the viewport.
    if (impl_->frame_rect_changed_) {
        impl_->frame_rect_changed_ = false;

        auto f = GetFrame();
        impl_->controls_->SetViewSize(Size(f.width, f.height));
        // GUI has origin of Y axis at top, but renderer has it at bottom
        // so we need to convert coordinates.
        int y = context.screenHeight - (f.height + f.y);

        auto view = impl_->scene_.GetView(impl_->view_id_);
        view->SetViewport(f.x, y, f.width, f.height);

        auto* camera = GetCamera();
        float aspect = 1.0f;
        if (f.height > 0) {
            aspect = float(f.width) / float(f.height);
        }
        GetCamera()->SetProjection(camera->GetFieldOfView(), aspect,
                                   camera->GetNear(), camera->GetFar(),
                                   camera->GetFieldOfViewType());
    }

    // The actual drawing is done later, at the end of drawing in
    // Window::OnDraw(), in FilamentRenderer::Draw(). We can always
    // return NONE because any changes this frame will automatically
    // be rendered (unlike the ImGUI parts).
    return Widget::DrawResult::NONE;
}

Widget::EventResult SceneWidget::Mouse(const MouseEvent& e) {
    // Lower render quality while rotating, since we will be redrawing
    // frequently. This will give a snappier feel to mouse movements,
    // especially for point clouds, which are a little slow.
    if (e.type != MouseEvent::MOVE) {
        SetRenderQuality(Quality::FAST);
    }
    // Render quality will revert back to BEST after a short delay,
    // unless the user starts rotating again, or is scroll-wheeling.
    if (e.type == MouseEvent::DRAG || e.type == MouseEvent::WHEEL) {
        impl_->last_fast_time_ = Application::GetInstance().Now();
    }

    if (e.type == MouseEvent::BUTTON_DOWN) {
        impl_->buttons_down_ |= int(e.button.button);
    } else if (e.type == MouseEvent::BUTTON_UP) {
        impl_->buttons_down_ &= ~int(e.button.button);
    }

    impl_->controls_->Mouse(e);

    if (impl_->on_camera_changed_) {
        impl_->on_camera_changed_(GetCamera());
    }

    return Widget::EventResult::CONSUMED;
}

Widget::EventResult SceneWidget::Key(const KeyEvent& e) {
    impl_->controls_->Key(e);

    if (impl_->on_camera_changed_) {
        impl_->on_camera_changed_(GetCamera());
    }
    return Widget::EventResult::CONSUMED;
}

Widget::DrawResult SceneWidget::Tick(const TickEvent& e) {
    auto result = impl_->controls_->Tick(e);
    // If Tick() redraws, then a key is down. Make sure we are rendering
    // FAST and mark the time so that we don't timeout and revert back
    // to slow rendering before the key up happens.
    if (result == Widget::DrawResult::REDRAW) {
        SetRenderQuality(Quality::FAST);
        impl_->last_fast_time_ = Application::GetInstance().Now();
    }
    if (impl_->buttons_down_ == 0 && GetRenderQuality() == Quality::FAST) {
        double now = Application::GetInstance().Now();
        if (now - impl_->last_fast_time_ > DELAY_FOR_BEST_RENDERING_SECS) {
            SetRenderQuality(Quality::BEST);
            result = Widget::DrawResult::REDRAW;
        }
    }
    return result;
}

}  // namespace gui
}  // namespace open3d
