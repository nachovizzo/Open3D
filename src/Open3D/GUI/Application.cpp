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

#include "Open3D/GUI/Application.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_set>

#include "Open3D/GUI/Button.h"
#include "Open3D/GUI/Events.h"
#include "Open3D/GUI/Label.h"
#include "Open3D/GUI/Layout.h"
#include "Open3D/GUI/Native.h"
#include "Open3D/GUI/Task.h"
#include "Open3D/GUI/Theme.h"
#include "Open3D/GUI/Window.h"
#include "Open3D/Utility/Console.h"
#include "Open3D/Utility/FileSystem.h"
#include "Open3D/Visualization/Rendering/Filament/FilamentEngine.h"

namespace {

const double RUNLOOP_DELAY_SEC = 0.010;

std::string FindResourcePath(int argc, const char *argv[]) {
    std::string argv0;
    if (argc != 0 && argv) {
        argv0 = argv[0];
    }

    // Convert backslash (Windows) to forward slash
    for (auto &c : argv0) {
        if (c == '\\') {
            c = '/';
        }
    }

    // Chop off the process name
    auto last_slash = argv0.rfind("/");
    auto path = argv0.substr(0, last_slash);

    if (argv0[0] == '/' ||
        (argv0.size() > 3 && argv0[1] == ':' && argv0[2] == '/')) {
        // is absolute path, we're done
    } else {
        // relative path:  prepend working directory
        auto cwd = open3d::utility::filesystem::GetWorkingDirectory();
        path = cwd + "/" + path;
    }

#ifdef __APPLE__
    if (path.rfind("MacOS") == path.size() - 5) {  // path is in a bundle
        return path.substr(0, path.size() - 5) + "Resources";
    }
#endif  // __APPLE__

    auto resource_path = path + "/resources";
    if (!open3d::utility::filesystem::DirectoryExists(resource_path)) {
        return path + "/../resources";  // building with Xcode
    }
    return resource_path;
}

}  // namespace

namespace open3d {
namespace gui {

struct Application::Impl {
    std::string resource_path_;
    Theme theme_;
    double last_time_ = 0.0;
    bool is_GLFW_initalized_ = false;
    bool is_running_ = false;
    bool should_quit_ = false;

    std::shared_ptr<Menu> menubar_;
    std::unordered_set<std::shared_ptr<Window>> windows_;
    std::unordered_set<std::shared_ptr<Window>> windows_to_be_destroyed_;

    std::list<Task> running_tasks_;  // always accessed from main thread
    // ----
    struct Posted {
        Window *window;
        std::function<void()> f;

        Posted(Window *w, std::function<void()> func) : window(w), f(func) {}
    };
    std::mutex posted_lock_;
    std::vector<Posted> posted_;
    // ----

    void InitGFLW() {
        if (this->is_GLFW_initalized_) {
            return;
        }

#if __APPLE__
        glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);  // no auto-create menubar
#endif
        glfwInit();
        this->is_GLFW_initalized_ = true;
    }
};

Application &Application::GetInstance() {
    static Application g_app;
    return g_app;
}

void Application::ShowMessageBox(const char *title, const char *message) {
    utility::LogInfo("{}", message);

    auto alert = std::make_shared<Window>((title ? title : "Alert"),
                                          Window::FLAG_TOPMOST);
    auto em = alert->GetTheme().font_size;
    auto layout = std::make_shared<Vert>(em, Margins(em));
    auto msg = std::make_shared<Label>(message);
    auto ok = std::make_shared<Button>("Ok");
    ok->SetOnClicked([alert = alert.get() /*avoid shared_ptr cycle*/]() {
        Application::GetInstance().RemoveWindow(alert);
    });
    layout->AddChild(Horiz::MakeCentered(msg));
    layout->AddChild(Horiz::MakeCentered(ok));
    alert->AddChild(layout);
    Application::GetInstance().AddWindow(alert);
}

Application::Application() : impl_(new Application::Impl()) {
    Color highlight_color(0.5, 0.5, 0.5);

    // Note that any values here need to be scaled by the scale factor in Window
    impl_->theme_.font_path =
            "Roboto-Medium.ttf";   // full path will be added in Initialize()
    impl_->theme_.font_size = 16;  // 1 em (font size is em in digital type)
    impl_->theme_.default_margin = 8;          // 0.5 * em
    impl_->theme_.default_layout_spacing = 6;  // 0.333 * em

    impl_->theme_.background_color = Color(0.175, 0.175, 0.175);
    impl_->theme_.text_color = Color(0.875, 0.875, 0.875);
    impl_->theme_.border_width = 1;
    impl_->theme_.border_radius = 3;
    impl_->theme_.border_color = Color(0.5, 0.5, 0.5);
    impl_->theme_.menubar_border_color = Color(0.25, 0.25, 0.25);
    impl_->theme_.button_color = Color(0.4, 0.4, 0.4);
    impl_->theme_.button_hover_color = Color(0.6, 0.6, 0.6);
    impl_->theme_.button_active_color = Color(0.5, 0.5, 0.5);
    impl_->theme_.button_on_color = Color(0.7, 0.7, 0.7);
    impl_->theme_.button_on_hover_color = Color(0.9, 0.9, 0.9);
    impl_->theme_.button_on_active_color = Color(0.8, 0.8, 0.8);
    impl_->theme_.button_on_text_color = Color(0, 0, 0);
    impl_->theme_.checkbox_background_off_color = Color(0.333, 0.333, .333);
    impl_->theme_.checkbox_background_on_color = highlight_color;
    impl_->theme_.checkbox_background_hover_off_color = Color(0.5, 0.5, 0.5);
    impl_->theme_.checkbox_background_hover_on_color =
            highlight_color.Lightened(0.15);
    impl_->theme_.checkbox_check_color = Color(1, 1, 1);
    impl_->theme_.combobox_background_color = Color(0.4, 0.4, 0.4);
    impl_->theme_.combobox_hover_color = Color(0.5, 0.5, 0.5);
    impl_->theme_.combobox_arrow_background_color = highlight_color;
    impl_->theme_.slider_grab_color = Color(0.666, 0.666, 0.666);
    impl_->theme_.text_edit_background_color = Color(0.25, 0.25, 0.25);
    impl_->theme_.tab_inactive_color = impl_->theme_.button_color;
    impl_->theme_.tab_hover_color = impl_->theme_.button_hover_color;
    impl_->theme_.tab_active_color = impl_->theme_.button_active_color;
    impl_->theme_.dialog_border_width = 1;
    impl_->theme_.dialog_border_radius = 10;

    visualization::EngineInstance::SelectBackend(
            filament::backend::Backend::OPENGL);

    // Init GLFW here so that we can create windows before running
    impl_->InitGFLW();
}

Application::~Application() {}

void Application::Initialize() {
    // We don't have a great way of getting the process name, so let's hope that
    // the current directory is where the resources are located. This is a
    // safe assumption when running on macOS and Windows normally.
    auto path = open3d::utility::filesystem::GetWorkingDirectory();
    // Copy to C string, as some implementations of std::string::c_str()
    // return a very temporary pointer.
    char *argv = strdup(path.c_str());
    Initialize(1, (const char **)&argv);
    free(argv);
}

void Application::Initialize(int argc, const char *argv[]) {
    impl_->resource_path_ = FindResourcePath(argc, argv);
    impl_->theme_.font_path =
            impl_->resource_path_ + "/" + impl_->theme_.font_path;
}

double Application::Now() const { return glfwGetTime(); }

std::shared_ptr<Menu> Application::GetMenubar() const {
    return impl_->menubar_;
}

void Application::SetMenubar(std::shared_ptr<Menu> menubar) {
    auto old = impl_->menubar_;
    impl_->menubar_ = menubar;
    // If added or removed menubar, the size of the window's content region
    // may have changed (in not on macOS), so need to relayout.
    if ((!old && menubar) || (old && !menubar)) {
        for (auto w : impl_->windows_) {
            w->OnResize();
        }
    }

#if defined(__APPLE__)
    auto *native = menubar->GetNativePointer();
    if (native) {
        SetNativeMenubar(native);
    }
#endif  // __APPLE__
}

void Application::AddWindow(std::shared_ptr<Window> window) {
    window->OnResize();  // so we get an initial resize
    window->Show();
    impl_->windows_.insert(window);
}

void Application::RemoveWindow(Window *window) {
    for (auto it = impl_->windows_.begin(); it != impl_->windows_.end(); ++it) {
        if (it->get() == window) {
            impl_->windows_to_be_destroyed_.insert(*it);
            impl_->windows_.erase(it);
            break;
        }
    }

    if (impl_->windows_.empty()) {
        impl_->should_quit_ = true;
    }
}

void Application::Quit() {
    while (!impl_->windows_.empty()) {
        RemoveWindow(impl_->windows_.begin()->get());
    }
}

void Application::OnTerminate() {
    Quit();
    impl_->windows_to_be_destroyed_.clear();
}

void Application::OnMenuItemSelected(Menu::ItemId itemId) {
    for (auto w : impl_->windows_) {
        if (w->IsActiveWindow()) {
            w->OnMenuItemSelected(itemId);
            // This is a menu selection that came from a native menu.
            // We need to draw twice to ensure that any new dialog
            // that the menu item may have displayed is properly laid out.
            // (ImGUI can take up to two iterations to fully layout)
            // If we post two expose events they get coalesced, but
            // setting needsLayout forces two (for the reason given above).
            w->SetNeedsLayout();
            Window::UpdateAfterEvent(w.get());
            return;
        }
    }
}

void Application::Run() {
    while (RunOneTick())
        ;
}

bool Application::RunOneTick() {
    // Initialize if we have not started yet
    if (!impl_->is_running_) {
        // Verify that the resource path is valid. If it is not, display a
        // message box (std::cerr may not be visible to the user, if we were run
        // as app).
        if (impl_->resource_path_.empty()) {
            ShowNativeAlert(
                    "Internal error: Application::Initialize() was not called");
            return false;
        }
        if (!utility::filesystem::DirectoryExists(impl_->resource_path_)) {
            std::stringstream err;
            err << "Could not find resource directory:\n'"
                << impl_->resource_path_ << "' does not exist";
            ShowNativeAlert(err.str().c_str());
            return false;
        }
        if (!utility::filesystem::FileExists(impl_->theme_.font_path)) {
            std::stringstream err;
            err << "Could not load UI font:\n'" << impl_->theme_.font_path
                << "' does not exist";
            ShowNativeAlert(err.str().c_str());
            return false;
        }

        // We already called this in the constructor, but it is possible
        // (but unlikely) that the run loop finished and is starting again.
        impl_->InitGFLW();

        impl_->is_running_ = true;
    }

    // Process the events that have queued up
    auto status = ProcessQueuedEvents();

    // Cleanup if we are done
    if (status == RunStatus::DONE) {
        // Clear all the running tasks. The destructor will wait for them to
        // finish.
        for (auto it = impl_->running_tasks_.begin();
             it != impl_->running_tasks_.end(); ++it) {
            auto current = it;
            ++it;
            impl_->running_tasks_.erase(current);  // calls join()
        }

        glfwTerminate();
        impl_->is_GLFW_initalized_ = false;
        impl_->is_running_ = false;
    }

    return impl_->is_running_;
}

Application::RunStatus Application::ProcessQueuedEvents() {
    glfwWaitEventsTimeout(RUNLOOP_DELAY_SEC);

    // Handle tick messages.
    double now = Now();
    if (now - impl_->last_time_ >= 0.95 * RUNLOOP_DELAY_SEC) {
        for (auto w : impl_->windows_) {
            if (w->OnTickEvent(TickEvent())) {
                w->PostRedraw();
            }
        }
        impl_->last_time_ = now;
    }

    // Run any posted functions
    {
        std::lock_guard<std::mutex> lock(impl_->posted_lock_);
        for (auto &p : impl_->posted_) {
            void *old = nullptr;
            if (p.window) {
                old = p.window->MakeDrawContextCurrent();
            }
            p.f();
            if (p.window) {
                p.window->RestoreDrawContext(old);
                p.window->PostRedraw();
            }
        }
        impl_->posted_.clear();
    }

    // Clear any tasks that have finished
    impl_->running_tasks_.remove_if(
            [](const Task &t) { return t.IsFinished(); });

    // We can't destroy a GLFW window in a callback, so we need to do it here.
    // Since these are the only copy of the shared pointers, this will cause
    // the Window destructor to be called.
    impl_->windows_to_be_destroyed_.clear();

    if (impl_->should_quit_) {
        return RunStatus::DONE;
    }
    return RunStatus::CONTINUE;
}

void Application::RunInThread(std::function<void()> f) {
    // We need to be on the main thread here.
    impl_->running_tasks_.emplace_back(f);
    impl_->running_tasks_.back().Run();
}

void Application::PostToMainThread(Window *window, std::function<void()> f) {
    std::lock_guard<std::mutex> lock(impl_->posted_lock_);
    impl_->posted_.emplace_back(window, f);
}

const char *Application::GetResourcePath() const {
    return impl_->resource_path_.c_str();
}

const Theme &Application::GetTheme() const { return impl_->theme_; }

}  // namespace gui
}  // namespace open3d
