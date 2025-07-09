#include "picker_app.h"
#include <memory>
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
        app{grey::app::make(title, WindowMinWidth, WindowHeight)}, wnd_main{ title, &is_open } {
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

        // copy visible browsers and profiles
        for(const shared_ptr<browser> b : g_config.browsers) {
            if(b->is_hidden) continue;

            // count number of instances that are not hidden
            int visible_instances{0};
            for(const auto c : b->instances) {
                if(!c->is_hidden) {
                    visible_instances++;
                }
            }

            // skip browser if no visible instances
            if(visible_instances == 0) continue;

            browser bc = *b;
            bc.instances.clear();
            for(const auto bi : b->instances) {
                if(!bi->is_hidden) {
                    bc.instances.push_back(make_shared<browser_instance>(*bi));
                }
            }
            browsers.push_back(bc);
        }

        app->on_initialised = [this]() {
            app->preload_texture("incognito", incognito_icon_png, incognito_icon_png_len);
            app->preload_texture("more", picker_more_icon_png, picker_more_icon_png_len);

            // for each browser, get instances
            for(auto& b : browsers) {
                string path = b.get_best_icon_path();
                app->preload_texture(path, fss::get_full_path(path));

                for(auto bi : b.instances) {
                    string path = bi->get_best_icon_path();
                    app->preload_texture(path, fss::get_full_path(path));
                }
            }

            // Calculate window size based on profile count
            int total_profiles = 0;
            for(auto& b : browsers) {
                total_profiles += b.instances.size();
            }
            
            int cols = min(ProfileGridColumns, total_profiles);
            int rows = (total_profiles + cols - 1) / cols;
            
            wnd_width = max(WindowMinWidth, cols * (ProfileCardWidth + ProfileCardPadding) + ProfileCardPadding);
            wnd_height = ProfileHeaderHeight + rows * (ProfileCardHeight + ProfileCardPadding) + ProfileCardPadding + ProfileFooterHeight;
            
            app->resize_main_viewport(wnd_width, wnd_height);

            wnd_main
                .no_titlebar()
                .no_resize()
                .border(0)
                .fill_viewport()
                .no_scroll();
        };
    }

    picker_app::~picker_app() {
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

        return
            (g_config.picker_on_key_as && (k_alt && k_shift)) ||
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

        // URL editor
        ImGui::PushItemWidth(-1);
        w::sl();
        w::input(url, "##url");
        ImGui::PopItemWidth();
        w::tooltip(bt::strings::PickerUrlTooltip);

        render_header();
        render_profile_grid();
        render_footer();

        // close on Escape key
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            is_open = false;
        }

        return is_open;
    }

    void picker_app::render_header() {
        float x, y;
        w::get_pos(x, y);
        
        // Header section with main browser info
        w::set_pos(x, y + ProfileHeaderPadding);
        
        // Center the header content
        float window_width = ImGui::GetWindowWidth();
        float header_width = 400; // Estimated width for header content
        float header_x = (window_width - header_width) / 2;
        
        w::set_pos(header_x, y + ProfileHeaderPadding);
        
        // Main browser icon (use first browser as representative)
        if(!browsers.empty()) {
            auto& main_browser = browsers[0];
            float icon_size = 64;
            w::image(*app, main_browser.get_best_icon_path(), icon_size, icon_size);
            w::sl();
        }
        
        // Header text
        w::move_pos(20, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
        
        auto style = ImGui::GetStyle();
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use larger font if available
        w::label("Who's using Browser Tamer?", w::emphasis::primary);
        ImGui::PopFont();
        
        w::label("Choose a profile to continue browsing", w::emphasis::secondary);
        ImGui::PopStyleVar();
        
        w::set_pos(0, y + ProfileHeaderHeight);
    }

    void picker_app::render_profile_grid() {
        float x, y;
        w::get_pos(x, y);
        
        float window_width = ImGui::GetWindowWidth();
        int total_profiles = 0;
        for(auto& b : browsers) {
            total_profiles += b.instances.size();
        }
        
        int cols = min(ProfileGridColumns, total_profiles);
        float grid_width = cols * (ProfileCardWidth + ProfileCardPadding) - ProfileCardPadding;
        float grid_start_x = (window_width - grid_width) / 2;
        
        int current_col = 0;
        int current_row = 0;
        int profile_idx = 0;
        
        for(auto& browser : browsers) {
            for(auto& profile : browser.instances) {
                if(current_col >= ProfileGridColumns) {
                    current_col = 0;
                    current_row++;
                }
                
                float card_x = grid_start_x + current_col * (ProfileCardWidth + ProfileCardPadding);
                float card_y = y + current_row * (ProfileCardHeight + ProfileCardPadding) + ProfileCardPadding;
                
                render_profile_card(browser, profile, card_x, card_y, profile_idx);
                
                current_col++;
                profile_idx++;
            }
        }
    }

    void picker_app::render_profile_card(bt::browser& browser, std::shared_ptr<bt::browser_instance> profile, float x, float y, int index) {
        w::set_pos(x, y);
        
        ImVec2 card_size(ProfileCardWidth, ProfileCardHeight);
        ImVec2 card_min = ImGui::GetCursorScreenPos();
        ImVec2 card_max = ImVec2(card_min.x + card_size.x, card_min.y + card_size.y);
        
        // Card background
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImU32 card_bg_color = w::imcol32(ImGuiCol_FrameBg);
        ImU32 card_border_color = w::imcol32(ImGuiCol_Border);
        ImU32 card_hover_color = w::imcol32(ImGuiCol_FrameBgHovered);
        
        // Check if hovered or selected
        bool is_hovered = ImGui::IsMouseHoveringRect(card_min, card_max);
        bool is_keyed = keyboard_selection_idx == index;
        
        if(is_hovered || is_keyed) {
            draw_list->AddRectFilled(card_min, card_max, card_hover_color, ProfileCardRounding);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else {
            draw_list->AddRectFilled(card_min, card_max, card_bg_color, ProfileCardRounding);
        }
        
        draw_list->AddRect(card_min, card_max, card_border_color, ProfileCardRounding);
        
        // Create invisible button for interaction
        ImGui::SetCursorScreenPos(card_min);
        ImGui::InvisibleButton(fmt::format("profile_card_{}", index).c_str(), card_size);
        
        // Handle click
        if((ImGui::IsItemClicked() || is_keyed) && !is_keyed) {
            make_decision(profile);
            return;
        }
        
        // Profile avatar (centered, large)
        float avatar_size = 80;
        float avatar_x = x + (ProfileCardWidth - avatar_size) / 2;
        float avatar_y = y + 30;
        
        w::set_pos(avatar_x, avatar_y);
        
        if(profile->is_incognito) {
            w::image(*app, "incognito", avatar_size, avatar_size);
        } else {
            string path = profile->get_best_icon_path();
            w::rounded_image(*app, path, avatar_size, avatar_size, avatar_size / 2);
        }
        
        // Profile name (centered)
        float text_y = avatar_y + avatar_size + 15;
        w::set_pos(x + ProfileCardPadding, text_y);
        
        ImGui::PushItemWidth(ProfileCardWidth - 2 * ProfileCardPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
        
        // Profile name - center aligned
        float text_width = ImGui::CalcTextSize(profile->name.c_str()).x;
        float text_x = x + (ProfileCardWidth - text_width) / 2;
        w::set_pos(text_x, text_y);
        w::label(profile->name, w::emphasis::primary);
        
        // Browser name - center aligned, smaller
        string browser_text = browser.name;
        float browser_text_width = ImGui::CalcTextSize(browser_text.c_str()).x;
        float browser_text_x = x + (ProfileCardWidth - browser_text_width) / 2;
        w::set_pos(browser_text_x, text_y + 20);
        w::label(browser_text, w::emphasis::secondary);
        
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();
        
        // Browser icon (small, bottom-right corner)
        float browser_icon_size = 20;
        float browser_icon_x = card_max.x - browser_icon_size - 8;
        float browser_icon_y = card_max.y - browser_icon_size - 8;
        
        w::set_pos(browser_icon_x, browser_icon_y);
        w::image(*app, browser.get_best_icon_path(), browser_icon_size, browser_icon_size);
        
        // Tooltip
        if(is_hovered) {
            w::tooltip(fmt::format("{}\n{}", profile->name, browser.name));
        }
    }

    void picker_app::render_footer() {
        float x, y;
        w::get_pos(x, y);
        
        // Footer with additional options
        float window_width = ImGui::GetWindowWidth();
        float footer_y = y + ProfileFooterPadding;
        
        w::set_pos(ProfileCardPadding, footer_y);
        
        // Guest mode button (left side)
        if(ImGui::Button(" Guest mode")) {
            // Handle guest mode
        }
        
        w::sl();
        
        // Add profile button (center-ish)
        float add_button_x = (window_width - 100) / 2;
        w::set_pos(add_button_x, footer_y);
        
        if(ImGui::Button("+ Add Profile")) {
            // Handle add profile
        }
        
        // Settings/More options (right side)
        float settings_x = window_width - 100;
        w::set_pos(settings_x, footer_y);
        
        render_action_menu(settings_x, footer_y);
    }

    void picker_app::render_action_menu(float x, float y) {
        w::set_pos(x, y);
        
        if(ImGui::Button(ICON_MD_MORE_VERT " More")) {
            // Handle more options menu
        }
        
        if(ImGui::IsItemHovered()) {
            action_menu_hovered = true;
            
            // Simple dropdown for now
            if(ImGui::BeginPopupContextItem("more_menu")) {
                if(ImGui::Selectable("Copy URL")) {
                    menu_item_clicked("copy");
                }
                if(ImGui::Selectable("Email URL")) {
                    menu_item_clicked("email");
                }
                ImGui::EndPopup();
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
}
