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

#include "Open3D/GUI/NumberEdit.h"

#include <imgui.h>
#include <string.h>   // for snprintf
#include <algorithm>  // for min, max
#include <cmath>
#include <sstream>
#include <unordered_set>

#include "Open3D/GUI/Theme.h"
#include "Open3D/GUI/Util.h"

namespace open3d {
namespace gui {

namespace {
static int g_next_number_edit_id = 1;
}

struct NumberEdit::Impl {
    std::string id_;
    NumberEdit::Type type_;
    // Double has 53-bits of integer range, which is sufficient for the
    // numbers that are reasonable for users to be entering. (Since JavaScript
    // only uses doubles, apparently it works for a lot more situations, too.)
    double value_;
    double min_value_ = -2e9;  // -2 billion, which is roughly -INT_MAX
    double max_value_ = 2e9;
    int num_decimal_digits_ = -1;
    std::function<void(double)> on_changed_;
};

NumberEdit::NumberEdit(Type type) : impl_(new NumberEdit::Impl()) {
    impl_->type_ = type;
    std::stringstream s;
    s << "##numedit" << g_next_number_edit_id++;
    impl_->id_ = s.str();
}

NumberEdit::~NumberEdit() {}

int NumberEdit::GetIntValue() const { return int(impl_->value_); }

double NumberEdit::GetDoubleValue() const { return impl_->value_; }

void NumberEdit::SetValue(double val) {
    if (impl_->type_ == INT) {
        impl_->value_ = std::round(val);
    } else {
        impl_->value_ = val;
    }
}

double NumberEdit::GetMinimumValue() const { return impl_->min_value_; }

double NumberEdit::GetMaximumValue() const { return impl_->max_value_; }

void NumberEdit::SetLimits(double min_value, double max_value) {
    if (impl_->type_ == INT) {
        impl_->min_value_ = std::round(min_value);
        impl_->max_value_ = std::round(max_value);
    } else {
        impl_->min_value_ = min_value;
        impl_->max_value_ = min_value;
    }
    impl_->value_ = std::min(max_value, std::max(min_value, impl_->value_));
}

void NumberEdit::SetDecimalPrecision(int num_digits) {
    impl_->num_decimal_digits_ = num_digits;
}

void NumberEdit::SetOnValueChanged(std::function<void(double)> on_changed) {
    impl_->on_changed_ = on_changed;
}

Size NumberEdit::CalcPreferredSize(const Theme &theme) const {
    int num_min_digits = std::ceil(std::log10(std::abs(impl_->min_value_)));
    int num_max_digits = std::ceil(std::log10(std::abs(impl_->max_value_)));
    int num_digits = std::max(6, std::max(num_min_digits, num_max_digits));
    if (impl_->min_value_ < 0) {
        num_digits += 1;
    }

    auto pref = Super::CalcPreferredSize(theme);
    auto padding = pref.height - theme.font_size;
    return Size((num_digits * theme.font_size) / 2 + padding, pref.height);
}

Widget::DrawResult NumberEdit::Draw(const DrawContext &context) {
    auto &frame = GetFrame();
    ImGui::SetCursorPos(
            ImVec2(frame.x - context.uiOffsetX, frame.y - context.uiOffsetY));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                        0.0);  // macOS doesn't round text edit borders

    ImGui::PushStyleColor(
            ImGuiCol_FrameBg,
            util::colorToImgui(context.theme.text_edit_background_color));
    ImGui::PushStyleColor(
            ImGuiCol_FrameBgHovered,
            util::colorToImgui(context.theme.text_edit_background_color));
    ImGui::PushStyleColor(
            ImGuiCol_FrameBgActive,
            util::colorToImgui(context.theme.text_edit_background_color));

    auto result = Widget::DrawResult::NONE;
    DrawImGuiPushEnabledState();
    ImGui::PushItemWidth(GetFrame().width);
    if (impl_->type_ == INT) {
        int ivalue = impl_->value_;
        if (ImGui::InputInt(impl_->id_.c_str(), &ivalue)) {
            SetValue(ivalue);
            result = Widget::DrawResult::REDRAW;
        }
    } else {
        std::string fmt;
        if (impl_->num_decimal_digits_ >= 0) {
            char buff[32];
            snprintf(buff, 31, "%%.%df", impl_->num_decimal_digits_);
            fmt = buff;
        } else {
            if (impl_->value_ < 10) {
                fmt = "%.3f";
            } else if (impl_->value_ < 100) {
                fmt = "%.2f";
            } else if (impl_->value_ < 1000) {
                fmt = "%.1f";
            } else {
                fmt = "%.0f";
            }
        }
        double dvalue = impl_->value_;
        if (ImGui::InputDouble(impl_->id_.c_str(), &dvalue, 0.0, 0.0,
                               fmt.c_str())) {
            SetValue(dvalue);
            result = Widget::DrawResult::REDRAW;
        }
    }
    ImGui::PopItemWidth();
    DrawImGuiPopEnabledState();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (impl_->on_changed_) {
            impl_->on_changed_(impl_->value_);
        }
        result = Widget::DrawResult::REDRAW;
    }

    return result;
}

}  // namespace gui
}  // namespace open3d
