#define NOMINMAX
#include "picker_app.h"
#include <memory>
#include <algorithm>
#include "fss.h"
#include "../../globals.h"
#include "../../res.inl"
#include "fmt/core.h"
#include "../common/win32/user.h"
#include "../strings.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "../../common/win32/clipboard.h"
#include "../../common/win32/shell.h"

using namespace std;
namespace w = grey::widgets;

namespace bt::ui {
    picker_app::picker_app(const string& url)
        : url{url}, title{APP_LONG_NAME " - Pick"},
        app{grey::app::make(title, 1100, 800)}, wnd_main{ title, &is_open } {
        app->initial_theme_id = g_config.theme_id;
        app->win32_can_resize = true; // Enable resizing for responsive behavior
        app->win32_center_on_screen = true;
        app->win32_close_on_focus_lost = g_config.picker_close_on_focus_loss;
        app->win32_always_on_top = g_config.picker_always_on_top;

        // Process URL with pipeline
        {
            click_payload up{url};
            g_pipeline.process(up);
            this->url = up.url;
        }

        // Collect all visible profiles from all browsers
        for(const shared_ptr<browser> b : g_config.browsers) {
            if(b->is_hidden) continue;
            for(const auto& bi : b->instances) {
                if(bi->is_hidden) continue;
                ProfileCard card;
                card.instance = std::make_shared<browser_instance>(*bi);
                card.browser_icon_path = b->get_best_icon_path();
                card.profile_name = bi->name;
                card.profile_icon_path = bi->get_best_icon_path();
                card.is_incognito = bi->is_incognito;
                profile_cards.push_back(card);
            }
        }

        app->on_initialised = [this]() {
            // Enhanced ImGui style for translucent glass effect
            ImGuiStyle& style = ImGui::GetStyle();
            
            // Modern glass morphism styling
            style.WindowRounding = 24.0f;
            style.ChildRounding = 20.0f;
            style.FrameRounding = 16.0f;
            style.PopupRounding = 16.0f;
            style.ScrollbarRounding = 12.0f;
            style.GrabRounding = 12.0f;
            style.TabRounding = 12.0f;
            
            // Enhanced padding and spacing
            style.WindowPadding = ImVec2(32.0f, 32.0f);
            style.FramePadding = ImVec2(16.0f, 12.0f);
            style.ItemSpacing = ImVec2(20.0f, 16.0f);
            style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);
            style.IndentSpacing = 24.0f;
            style.ScrollbarSize = 1.0f;
            style.GrabMinSize = 12.0f;
            
            // Translucent color scheme with glass effect
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 1.0f, 0.12f); // Semi-transparent background
            style.Colors[ImGuiCol_ChildBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
            style.Colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.98f, 1.0f, 0.95f);
            
            // Enhanced frame colors for glass effect
            style.Colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.15f);
            style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.25f);
            style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.35f);
            
            // Modern button styling
            style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.20f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.23f, 0.51f, 0.96f, 0.35f);
            
            // Enhanced text colors
            style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
            style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.70f, 0.70f, 0.75f, 0.8f);
            
            // Border and separator colors
            style.Colors[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 1.0f, 0.15f);
            style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            style.Colors[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
            
            // Enhanced selection and header colors
            style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.18f);
            style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.23f, 0.51f, 0.96f, 0.30f);
            
            app->preload_texture("incognito", incognito_icon_png, incognito_icon_png_len);
            app->preload_texture("more", picker_more_icon_png, picker_more_icon_png_len);
            for(const auto& card : profile_cards) {
                app->preload_texture(card.browser_icon_path, fss::get_full_path(card.browser_icon_path));
                app->preload_texture(card.profile_icon_path, fss::get_full_path(card.profile_icon_path));
            }
            
            // Set minimum window size for responsive behavior
            calculate_responsive_layout();
            wnd_main.no_titlebar().border(0).fill_viewport().no_scroll();
        };
    }

    picker_app::~picker_app() {}

    void picker_app::calculate_responsive_layout() {
        if(profile_cards.empty()) return;
        
        // Calculate responsive grid based on profile count
        int profile_count = static_cast<int>(profile_cards.size());
        int ideal_cols = std::min(4, static_cast<int>(std::ceil(std::sqrt(profile_count))));
        
        // Adjust columns based on window width
        float window_width = ImGui::GetIO().DisplaySize.x;
        if(window_width < 600.0f) {
            ProfilesPerRow = 2;
        } else if(window_width < 900.0f) {
            ProfilesPerRow = 3;
        } else {
            ProfilesPerRow = ideal_cols;
        }
        
        int rows = (profile_count + ProfilesPerRow - 1) / ProfilesPerRow;
        float card_size = 140.0f * app->scale;
        float spacing = 24.0f * app->scale;
        
        float grid_width = ProfilesPerRow * card_size + (ProfilesPerRow + 1) * spacing;
        float grid_height = rows * (card_size + 40.0f) + (rows + 1) * spacing + 120.0f;
        
        // Set minimum dimensions
        float min_width = std::max(480.0f, grid_width + 64.0f);
        float min_height = std::max(360.0f, grid_height + 64.0f);
        
        app->resize_main_viewport(min_width, min_height);
    }

    std::shared_ptr<bt::browser_instance> picker_app::run() {
        app->run([this](const grey::app& app) {
            return run_frame();
        });
        return decision;
    }

    bool picker_app::is_hotkey_down() {
        bool k_shift = win32::user::is_kbd_shift_down();
        bool k_ctrl = win32::user::is_kbd_ctrl_down();
        bool k_alt = win32::user::is_kbd_alt_down();
        bool k_caps = win32::user::is_kbd_caps_locks_on();
        return (g_config.picker_on_key_as && (k_alt && k_shift)) ||
            (g_config.picker_on_key_ca && (k_ctrl && k_alt)) ||
            (g_config.picker_on_key_cs && (k_ctrl && k_shift)) ||
            (g_config.picker_on_key_cl && k_caps);
    }

    bool picker_app::run_frame() {
        w::guard gw{wnd_main};
        
        // Handle window resize for responsive behavior
        static float last_window_width = 0.0f;
        float current_width = ImGui::GetWindowWidth();
        if(std::abs(current_width - last_window_width) > 10.0f) {
            calculate_responsive_layout();
            last_window_width = current_width;
        }
        
        keyboard_selection_idx = -1;
        for(int key_index = ImGuiKey_1; key_index <= ImGuiKey_9; key_index++) {
            if(ImGui::IsKeyPressed((ImGuiKey)key_index)) {
                keyboard_selection_idx = key_index - ImGuiKey_1;
                break;
            }
        }
        
        // Enhanced gradient background with glass effect
        render_glass_background();
        
        // URL input with modern styling
        render_url_input();
        
        // Render the responsive profile grid
        render_profile_grid(ImGui::GetWindowWidth());
        
        // Close on Escape key
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            is_open = false;
        }
        
        return is_open;
    }

    void picker_app::render_glass_background() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 win_min = ImGui::GetWindowPos();
        ImVec2 win_max = ImGui::GetWindowContentRegionMax();
        win_max.x += win_min.x;
        win_max.y += win_min.y;
        
        // Multi-layer gradient background for depth
        dl->AddRectFilledMultiColor(
            win_min, win_max,
            IM_COL32(102, 126, 234, 255), // Deep blue top-left
            IM_COL32(118, 75, 162, 255),  // Purple top-right
            IM_COL32(59, 130, 246, 255),  // Lighter blue bottom-left
            IM_COL32(147, 51, 234, 255)   // Vibrant purple bottom-right
        );
        
        // Add subtle noise pattern for glass texture
        for(int i = 0; i < 50; i++) {
            float x = win_min.x + (rand() % static_cast<int>(win_max.x - win_min.x));
            float y = win_min.y + (rand() % static_cast<int>(win_max.y - win_min.y));
            dl->AddCircleFilled(ImVec2(x, y), 1.0f, IM_COL32(255, 255, 255, 15));
        }
    }

    void picker_app::render_url_input() {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20.0f, 16.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.35f));
        
        ImGui::PushItemWidth(-1);
        w::sl();
        w::input(url, "##url");
        ImGui::PopItemWidth();
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        
        w::tooltip(bt::strings::PickerUrlTooltip);
        ImGui::Spacing();
        ImGui::Spacing();
    }

    void picker_app::render_profile_grid(float win_width) {
        if(profile_cards.empty()) return;
        
        int rows = (static_cast<int>(profile_cards.size()) + ProfilesPerRow - 1) / ProfilesPerRow;
        float card_size = 140.0f * app->scale;
        float spacing = 24.0f * app->scale;
        float icon_size = 72.0f * app->scale;
        float badge_size = 28.0f * app->scale;
        
        // Calculate grid positioning
        float grid_width = ProfilesPerRow * card_size + (ProfilesPerRow - 1) * spacing;
        float start_x = (win_width - grid_width) / 2.0f;
        
        if(profiles_cb.size() != profile_cards.size()) {
            profiles_cb.resize(profile_cards.size());
        }
        
        int idx = 0;
        for(int row = 0; row < rows; row++) {
            for(int col = 0; col < ProfilesPerRow && idx < profile_cards.size(); col++) {
                auto& card = profile_cards[idx];
                
                float card_x = start_x + col * (card_size + spacing);
                float card_y = 120.0f + row * (card_size + 60.0f + spacing);
                
                render_profile_card(card, idx, card_x, card_y, card_size, icon_size, badge_size);
                idx++;
            }
        }
    }

    void picker_app::render_profile_card(const ProfileCard& card, int idx, float x, float y, 
                                        float card_size, float icon_size, float badge_size) {
        w::set_pos(x, y);
        ImGui::Dummy(ImVec2(card_size, card_size));
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 card_min = ImGui::GetItemRectMin();
        ImVec2 card_max = ImGui::GetItemRectMax();
        
        bool is_hovered = w::is_hovered();
        bool is_selected = idx == active_profile_idx;
        bool is_keyed = keyboard_selection_idx == idx;
        
        // Enhanced glass card effect with multiple layers
        float rounding = 20.0f * app->scale;
        
        // Drop shadow
        ImVec2 shadow_offset = ImVec2(0, 8.0f * app->scale);
        dl->AddRectFilled(
            ImVec2(card_min.x + shadow_offset.x, card_min.y + shadow_offset.y),
            ImVec2(card_max.x + shadow_offset.x, card_max.y + shadow_offset.y),
            IM_COL32(0, 0, 0, 25), rounding
        );
        
        // Card background with glass effect
        ImU32 card_bg;
        ImU32 border_color;
        
        if(is_selected || is_keyed) {
            card_bg = IM_COL32(59, 130, 246, 80);
            border_color = IM_COL32(59, 130, 246, 180);
        } else if(is_hovered) {
            card_bg = IM_COL32(255, 255, 255, 40);
            border_color = IM_COL32(255, 255, 255, 100);
        } else {
            card_bg = IM_COL32(255, 255, 255, 20);
            border_color = IM_COL32(255, 255, 255, 60);
        }
        
        dl->AddRectFilled(card_min, card_max, card_bg, rounding);
        dl->AddRect(card_min, card_max, border_color, rounding, 0, 2.0f);
        
        // Render profile icon
        float icon_x = x + (card_size - icon_size) / 2.0f;
        float icon_y = y + 20.0f * app->scale;
        
        w::set_pos(icon_x, icon_y);
        if(card.is_incognito) {
            w::image(*this->app, "incognito", icon_size, icon_size);
        } else {
            w::rounded_image(*this->app, card.profile_icon_path, icon_size, icon_size, icon_size / 2.0f);
        }
        
        // Browser badge
        float badge_x = icon_x + icon_size - badge_size * 0.6f;
        float badge_y = icon_y + icon_size - badge_size * 0.6f;
        w::set_pos(badge_x, badge_y);
        w::rounded_image(*this->app, card.browser_icon_path, badge_size, badge_size, badge_size / 2.0f);
        
        // Profile name with enhanced typography
        std::string display_name = card.profile_name;
        if(display_name.length() > 14) {
            display_name = display_name.substr(0, 11) + "...";
        }
        
        ImVec2 text_size = ImGui::CalcTextSize(display_name.c_str());
        float text_x = x + (card_size - text_size.x) / 2.0f;
        float text_y = icon_y + icon_size + 12.0f * app->scale;
        
        w::set_pos(text_x, text_y);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
        ImGui::TextUnformatted(display_name.c_str());
        ImGui::PopStyleColor();
        
        // Keyboard shortcut indicator
        if(idx < 9) {
            std::string shortcut = std::to_string(idx + 1);
            ImVec2 shortcut_size = ImGui::CalcTextSize(shortcut.c_str());
            float shortcut_x = card_max.x - shortcut_size.x - 12.0f * app->scale;
            float shortcut_y = card_min.y + 8.0f * app->scale;
            
            // Shortcut background
            ImVec2 shortcut_bg_min = ImVec2(shortcut_x - 6.0f, shortcut_y - 4.0f);
            ImVec2 shortcut_bg_max = ImVec2(shortcut_x + shortcut_size.x + 6.0f, shortcut_y + shortcut_size.y + 4.0f);
            dl->AddRectFilled(shortcut_bg_min, shortcut_bg_max, IM_COL32(255, 255, 255, 40), 6.0f);
            dl->AddRect(shortcut_bg_min, shortcut_bg_max, IM_COL32(255, 255, 255, 80), 6.0f, 0, 1.0f);
            
            w::set_pos(shortcut_x, shortcut_y);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
            ImGui::TextUnformatted(shortcut.c_str());
            ImGui::PopStyleColor();
        }
        
        // Handle interactions
        w::set_pos(x, y);
        ImGui::Dummy(ImVec2(card_size, card_size));
        
        if(is_hovered) {
            active_profile_idx = idx;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        
        w::tooltip(card.profile_name);
        
        bool is_clicked = w::is_leftclicked();
        if(is_clicked || is_keyed) {
            make_decision(card.instance);
        }
        
        profiles_cb[idx] = {card_min, card_max};
    }

    void picker_app::make_decision(std::shared_ptr<bt::browser_instance> decision) {
        this->decision = decision;
        is_open = false;
    }

    void picker_app::menu_item_clicked(const std::string& id) {
        if(id == "copy") {
            win32::clipboard::set_ascii_text(url);
            is_open = false;
        } else if(id == "email") {
            win32::clipboard::set_ascii_text(url);
            win32::shell::exec(fmt::format("mailto:?body={}", url), "");
            is_open = false;
        }
    }
}