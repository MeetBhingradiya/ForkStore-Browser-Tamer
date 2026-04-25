#pragma once

#include <string>

namespace bt::ui {
    std::string normalize_fluent2_theme_id(const std::string& theme_id);
    void apply_fluent2_theme(const std::string& theme_id, float scale);
}
