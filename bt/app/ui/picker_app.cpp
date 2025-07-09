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
        app{grey::app::make(title, 600, 500)}, wnd_main{ title, &is_open } {
        app->initial_theme_id = g_config.theme_id;
        app->win32_can_resize = false;
        app->win32_center_on_screen = true;
        app->win32_close_on_focus_lost = g_config.picker_close_on_focus_loss;
        app->win32_always_on_top = g_config.picker_always_on_top;

        // process URL with pipeline
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
            app->preload_texture("incognito", incognito_icon_png, incognito_icon_png_len);
            app->preload_texture("more", picker_more_icon_png, picker_more_icon_png_len);
            for(const auto& card : profile_cards) {
                app->preload_texture(card.browser_icon_path, fss::get_full_path(card.browser_icon_path));
                app->preload_texture(card.profile_icon_path, fss::get_full_path(card.profile_icon_path));
            }
            // Set window size for large grid
            int cols = ProfilesPerRow;
            int rows = (int(profile_cards.size()) + cols - 1) / cols;
            float grid_w = cols * ProfileCardSize + (cols + 1) * ProfileCardSpacing;
            float grid_h = rows * ProfileCardSize + (rows + 1) * ProfileCardSpacing + 80;
            app->resize_main_viewport(grid_w, grid_h);
            wnd_main.no_titlebar().no_resize().border(0).fill_viewport().no_scroll();
        };
    }

    picker_app::~picker_app() {}

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
        keyboard_selection_idx = -1;
        for(int key_index = ImGuiKey_1; key_index <= ImGuiKey_9; key_index++) {
            if(ImGui::IsKeyPressed((ImGuiKey)key_index)) {
                keyboard_selection_idx = key_index - ImGuiKey_1;
                break;
            }
        }
        // Chrome-style background
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 win_min = ImGui::GetWindowPos();
        ImVec2 win_max = {win_min.x + ImGui::GetWindowWidth(), win_min.y + ImGui::GetWindowHeight()};
        dl->AddRectFilled(win_min, win_max, IM_COL32(32, 33, 36, 255), 18.0f);
        // URL editor
        ImGui::PushItemWidth(-1);
        w::sl();
        w::input(url, "##url");
        ImGui::PopItemWidth();
        w::tooltip(bt::strings::PickerUrlTooltip);
        // Render the profile grid
        render_profile_grid();
        // Close on Escape key
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            is_open = false;
        }
        return is_open;
    }

    void picker_app::render_profile_grid() {
        int cols = ProfilesPerRow;
        int rows = (int(profile_cards.size()) + cols - 1) / cols;
        float card_size = 120.0f * app->scale; // Larger cards
        float card_padding = 16.0f * app->scale;
        float icon_size = 90.0f * app->scale;
        float badge_size = 32.0f * app->scale;
        float spacing = 24.0f * app->scale;
        float grid_w = cols * card_size + (cols + 1) * spacing;
        float start_x = (ImGui::GetWindowWidth() - grid_w) / 2.0f + spacing;
        float y = 60.0f * app->scale;
        if(profiles_cb.size() != profile_cards.size()) profiles_cb.resize(profile_cards.size());
        int idx = 0;
        for(int row = 0; row < rows; row++) {
            for(int col = 0; col < cols && idx < profile_cards.size(); col++) {
                auto& card = profile_cards[idx];
                float card_x = start_x + col * (card_size + spacing);
                float card_y = y + row * (card_size + spacing);
                w::set_pos(card_x, card_y);
                ImGui::Dummy(ImVec2(card_size, card_size));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 card_min = ImGui::GetItemRectMin();
                ImVec2 card_max = ImGui::GetItemRectMax();
                bool is_hovered = w::is_hovered();
                bool is_selected = idx == active_profile_idx;
                ImU32 card_bg = is_selected ? IM_COL32(60, 132, 246, 255) : (is_hovered ? IM_COL32(60,60,60,255) : IM_COL32(48,49,52,255));
                ImU32 border = is_selected ? IM_COL32(60, 132, 246, 255) : IM_COL32(80,80,80,255);
                float rounding = 18.0f * app->scale;
                dl->AddRectFilled(card_min, card_max, card_bg, rounding);
                dl->AddRect(card_min, card_max, border, rounding, 0, 2.0f);
                // Profile icon
                float icon_x = card_x + (card_size - icon_size) / 2;
                float icon_y = card_y + card_padding;
                w::set_pos(icon_x, icon_y);
                if(card.is_incognito) {
                    w::image(*app, "incognito", icon_size, icon_size);
                } else {
                    w::rounded_image(*app, card.profile_icon_path, icon_size, icon_size, icon_size / 2);
                }
                // Browser badge
                float badge_x = card_x + card_padding;
                float badge_y = card_y + card_size - badge_size - card_padding;
                w::set_pos(badge_x, badge_y);
                w::rounded_image(*app, card.browser_icon_path, badge_size, badge_size, badge_size / 2);
                // Profile name
                float name_y = icon_y + icon_size + card_padding / 2;
                std::string display_name = card.profile_name;
                if(display_name.length() > 16) display_name = display_name.substr(0, 13) + "...";
                ImVec2 text_size = ImGui::CalcTextSize(display_name.c_str());
                float text_x = card_x + (card_size - text_size.x) / 2;
                w::set_pos(text_x, name_y);
                ImGui::TextUnformatted(display_name.c_str());
                // Interactions
                w::set_pos(card_x, card_y);
                ImGui::Dummy(ImVec2(card_size, card_size));
                if(is_hovered) {
                    active_profile_idx = idx;
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
                w::tooltip(card.profile_name);
                bool is_keyed = keyboard_selection_idx == idx;
                bool is_clicked = w::is_leftclicked();
                if(is_clicked || is_keyed) {
                    make_decision(card.instance);
                }
                profiles_cb[idx] = {card_min, card_max};
                idx++;
            }
        }
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

    void picker_app::render_chrome_style_profiles(bt::browser& b) {
        float card_size = ProfileCardSize * app->scale;
        float card_padding = ProfileCardPadding * app->scale;
        float icon_size = ProfileIconSize * app->scale;
        float badge_size = BrowserBadgeSize * app->scale;
        float spacing = ProfileCardSpacing * app->scale;

        float x, y;
        w::get_pos(x, y);

        // Calculate grid layout
        int profile_count = b.instances.size();
        int rows = (profile_count + ProfilesPerRow - 1) / ProfilesPerRow;
        int cols = min(profile_count, ProfilesPerRow);

        // Calculate total width and centering
        float total_width = cols * card_size + (cols - 1) * spacing;
        float start_x = (wnd_width - total_width) / 2;

        // Resize profiles_cb to match instance count
        if(profiles_cb.size() != b.instances.size()) {
            profiles_cb.resize(b.instances.size());
        }

        int idx = 0;
        for(int row = 0; row < rows; row++) {
            for(int col = 0; col < cols && idx < profile_count; col++) {
                auto& profile = b.instances[idx];

                float card_x = start_x + col * (card_size + spacing);
                float card_y = y + row * (card_size + spacing);

                // Draw profile card background
                w::set_pos(card_x, card_y);
                ImGui::Dummy(ImVec2(card_size, card_size));

                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 card_min = ImGui::GetItemRectMin();
                ImVec2 card_max = ImGui::GetItemRectMax();

                // Card background and border
                bool is_hovered = w::is_hovered();
                bool is_selected = idx == active_profile_idx;
                profile->ui_is_hovered = is_hovered;

                ImU32 card_bg_color = is_selected ?
                    w::imcol32(ImGuiCol_ButtonActive) :
                    (is_hovered ? w::imcol32(ImGuiCol_ButtonHovered) : w::imcol32(ImGuiCol_Button));

                ImU32 border_color = is_selected ?
                    w::imcol32(ImGuiCol_ButtonActive) :
                    w::imcol32(ImGuiCol_Border);

                // Draw card with rounded corners
                float rounding = 8.0f * app->scale;
                dl->AddRectFilled(card_min, card_max, card_bg_color, rounding);
                dl->AddRect(card_min, card_max, border_color, rounding, 0, 1.0f);

                // Draw profile icon (centered in card)
                float icon_x = card_x + (card_size - icon_size) / 2;
                float icon_y = card_y + card_padding;

                w::set_pos(icon_x, icon_y);

                if(profile->is_incognito) {
                    w::image(*app, "incognito", icon_size, icon_size);
                } else {
                    string path = profile->get_best_icon_path();
                    w::rounded_image(*app, path, icon_size, icon_size, icon_size / 2);
                }

                // Draw browser badge in bottom-left corner
                float badge_x = card_x + card_padding;
                float badge_y = card_y + card_size - badge_size - card_padding;

                w::set_pos(badge_x, badge_y);
                string browser_icon_path = b.get_best_icon_path();
                w::rounded_image(*app, browser_icon_path, badge_size, badge_size, badge_size / 2);

                // Draw profile name below icon
                float name_y = icon_y + icon_size + card_padding / 2;
                float name_height = ImGui::GetTextLineHeight();

                // Truncate long names
                string display_name = profile->name;
                if(display_name.length() > 12) {
                    display_name = display_name.substr(0, 9) + "...";
                }

                // Center text horizontally
                ImVec2 text_size = ImGui::CalcTextSize(display_name.c_str());
                float text_x = card_x + (card_size - text_size.x) / 2;

                w::set_pos(text_x, name_y);
                ImGui::TextUnformatted(display_name.c_str());

                // Handle interactions
                w::set_pos(card_x, card_y);
                ImGui::Dummy(ImVec2(card_size, card_size));

                if(is_hovered) {
                    active_profile_idx = idx;
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                w::tooltip(profile->name);

                bool is_keyed = keyboard_selection_idx == idx;
                bool is_clicked = w::is_leftclicked();
                if(is_clicked || is_keyed) {
                    make_decision(profile);
                }

                // Store card bounds for connection lines
                profiles_cb[idx] = {card_min, card_max};

                idx++;
            }
        }

        // Update window position after profile grid
        w::set_pos(0, y + rows * (card_size + spacing));
    }
}
