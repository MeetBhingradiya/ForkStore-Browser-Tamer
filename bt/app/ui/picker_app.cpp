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
        app{grey::app::make(title, WindowMinWidth, WindowHeight)}, wnd_main{ title, &is_open } {
        app->initial_theme_id = g_config.theme_id;
        //app->load_icon_font = false;
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
            int max_instances{0};
            for(auto& b : browsers) {
                string path = b.get_best_icon_path();
                app->preload_texture(path, fss::get_full_path(path));

                for(auto bi : b.instances) {
                    string path = bi->get_best_icon_path();
                    app->preload_texture(path, fss::get_full_path(path));
                }

                if(b.instances.size() > max_instances) {
                    max_instances = b.instances.size();
                }
            }

            auto style = ImGui::GetStyle();

            wnd_width = (BrowserSquareSize + style.WindowPadding.x * 2 / app->scale) * browsers.size() +
                style.WindowPadding.x * 2 / app->scale;
            wnd_width = max(wnd_width, WindowMinWidth);
            
            // Ensure window is wide enough for profile cards
            if(max_instances > 1) {
                int profile_cols = min(max_instances, ProfilesPerRow);
                float profile_width = profile_cols * ProfileCardSize + (profile_cols - 1) * ProfileCardSpacing + ProfileCardSpacing * 2;
                wnd_width = max(wnd_width, profile_width);
            }

            // calculate window height based on maximum profiles
            float base_height = BrowserSquareSize + 80; // URL bar + browser bar + padding
            float profile_height = 0;
            if(max_instances > 1) {
                int profile_rows = (max_instances + ProfilesPerRow - 1) / ProfilesPerRow;
                profile_height = profile_rows * (ProfileCardSize + ProfileCardSpacing) + ProfileCardSpacing * 2;
            }
            float total_height = base_height + profile_height;

            // calculate lef pad so browsers look centered
            browser_bar_left_pad = (wnd_width - (BrowserSquareSize * browsers.size())) / 2 * app->scale;

            app->resize_main_viewport(wnd_width, total_height);

            wnd_main
                //.size(wnd_width, wnd_height_normal)
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

        // inspiration: https://github.com/sonnyp/Junction

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

        render_browser_bar();
        render_profile_bar();
        render_connection_box();

        // close on Escape key
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {

            if(active_profile_idx >= 0) {
                // close profile bar if open
                active_profile_idx = -1;
            } else if(active_browser_idx >= 0) {
                // close browser bar if open
                active_browser_idx = -1;
            } else {
                // close the whole window
                is_open = false;
            }
        }

        return is_open;
    }

    // Function to get the coordinates of a point on a circle
    void get_point_on_circle(float x_center, float y_center, float radius, float angle, float& x, float& y) {
        x = x_center + radius * std::cos(angle);
        y = y_center + radius * std::sin(angle);
    }

    void picker_app::render_action_menu(float x, float y) {

        if(!has_profile_bar) y += ProfileSquareSize * app->scale / 2;
        float sq_size = BrowserSquareSize * app->scale;
        ImVec2 wp = ImGui::GetWindowViewport()->WorkPos;
        float tlh = ImGui::GetTextLineHeight();
        float cx = x + sq_size / 2;
        float cy = y + sq_size / 2;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // occupy entire space
        w::set_pos(x, y);
        ImGui::Dummy(ImVec2(sq_size, sq_size));
        action_menu_hovered = w::is_hovered();

        // find center in absolute coordinates
        //auto img_rect_min = ImGui::GetItemRectMin();
        //auto img_rect_max = ImGui::GetItemRectMax();
        //float center_x = (img_rect_max.x + img_rect_min.x) / 2;
        //float center_y = (img_rect_max.y + img_rect_min.y) / 2;

        if(action_menu_hovered) {
            ImU32 col_bg = w::imcol32(ImGuiCol_MenuBarBg);
            //dl->AddCircleFilled(ImVec2(cx + wp.x, cy + wp.y), sq_size / 2, col_bg);
            //dl->PathArcTo(ImVec2(cx + wp.x, cy + wp.y), sq_size / 2 - 2, 0, M_PI);
        }

        // draw collapsed "action"
        {
            float x1 = x + sq_size / 2 - tlh / 2;
            float y1 = y + sq_size / 2 - tlh / 2;
            w::set_pos(x1, y1);
            w::label(ICON_MD_ADD_CIRCLE, 0, action_menu_hovered);
            if(w::is_hovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // draw menu items in a circle
        if(action_menu_hovered) {
            ImU32 col_dot = w::imcol32(ImGuiCol_Text);
            
            float angle = 0; // PI is half a circle

            for(action_menu_item& mi : action_menu_items) {
                if(mi.angle_final == 0) mi.angle_final = angle;

                if(mi.angle < mi.angle_final || mi.x == 0 || mi.y == 0) {
                    mi.angle += 0.1;
                    get_point_on_circle(cx, cy, sq_size / 4, mi.angle, mi.x, mi.y);
                }

                w::set_pos(mi.x, mi.y);
                w::label(mi.icon, w::emphasis::primary, 0);
                if(w::is_hovered())
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                w::tooltip(mi.tooltip);
                if(w::is_leftclicked()) {
                    menu_item_clicked(mi.id);
                }

                //dl->AddCircleFilled(ImVec2(x2 + wp.x, y2 + wp.y), 2, col_dot);

                angle += M_PI / action_menu_items.size();
            }
        } else {
            for(action_menu_item& mi : action_menu_items) {
                mi.angle = 0;
                mi.angle_final = 0;
            }
        }
    }

    void picker_app::render_browser_bar() {
        float sq_size = BrowserSquareSize * app->scale;
        float pad = BrowserSquarePadding * app->scale;
        float pad1 = InactiveBrowserSquarePadding * app->scale;
        float full_icon_size = sq_size - pad * 2;
        float full_icon_size1 = sq_size - pad1 * 2;
        float x, y;
        w::get_pos(x, y);

        int idx = 0;
        for(bt::browser& b : browsers) {

            float box_x_start = sq_size * idx + browser_bar_left_pad;
            float box_y_start = y;

            // pad a bit lower if there is no profile bar
            if(!has_profile_bar) box_y_start += ProfileSquareSize * app->scale / 2;

            w::set_pos(box_x_start, box_y_start);
            bool is_multiple_choice = b.instances.size() > 1;

            {
                w::group g;
                g.render();

                ImGui::Dummy(ImVec2(sq_size, sq_size));
                w::set_pos(box_x_start, box_y_start);

                if(b.ui_is_hovered || idx == active_browser_idx) {
                    w::move_pos(pad, pad);
                    w::image(*app, b.get_best_icon_path(), full_icon_size, full_icon_size);
                } else {
                    float pad1 = InactiveBrowserSquarePadding * app->scale;
                    float diff = pad1 - pad;
                    float isz = sq_size - pad1 * 2;

                    w::move_pos(pad1, pad1);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, InactiveAlpha);
                    w::image(*app, b.get_best_icon_path(), full_icon_size1, full_icon_size1);
                    ImGui::PopStyleVar();
                }

                // draw a number overlay
                //w::set_pos(box_x_start, box_y_start);
                //w::label(fmt::format("{}", idx));
            }

            // move to bottom-right corner
            w::set_pos(sq_size * idx + pad, sq_size);

            bool is_keyed = !has_profile_bar && keyboard_selection_idx == idx;
            if(is_keyed) {
                keyboard_selection_idx = -1;
            }
            bool is_moused = w::is_hovered();
            b.ui_is_hovered = is_keyed || is_moused;
            if(b.ui_is_hovered) {
                if(active_browser_idx != idx) {
                    active_profile_idx = -1;
                    profiles_cb.clear();
                }

                active_browser_idx = idx;
                if(!is_multiple_choice) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    if(w::is_leftclicked() || is_keyed) {
                        make_decision(b.instances[0]);
                    }
                }
            }
            w::tooltip(b.name);

            if(active_browser_idx == idx) {
                // this needs to be re-calculatd on each frame
                active_browser_cb.min = ImGui::GetItemRectMin();
                active_browser_cb.max = ImGui::GetItemRectMax();
            }

            if(idx < (browsers.size() - 1)) {
                w::sl();
            }

            idx++;
        }

        // "extra actions" area
        render_action_menu(sq_size * idx + browser_bar_left_pad, y);

        w::set_pos(0, y + BrowserSquareSize * app->scale);
    }

    void picker_app::render_profile_bar() {

        has_profile_bar = false;

        // profiles if required
        if(active_browser_idx >= 0) {
            auto& b = browsers[active_browser_idx];
            if(b.instances.size() > 1) {
                has_profile_bar = true;
                w::move_pos(0, ProfileSquarePadding * app->scale * 2);  // some distance from browser bar

                render_chrome_style_profiles(b);
            }
        }
    }

    void picker_app::render_connection_box() {
        if(active_browser_cb.min.x == 0 || active_browser_idx == -1 || !has_profile_bar) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col_dot = w::imcol32(ImGuiCol_Text);

        // calculate browser connection point (bottom center)
        ImVec2 browser_point{
            (active_browser_cb.min.x + active_browser_cb.max.x) / 2,
            active_browser_cb.max.y};

        // Draw connection lines to profile cards
        for(int i = 0; i < profiles_cb.size(); i++) {
            auto& cb = profiles_cb[i];
            
            // Profile card connection point (top center)
            ImVec2 profile_point{
                (cb.min.x + cb.max.x) / 2,
                cb.min.y};

            // Draw connection line
            float thickness = i == active_profile_idx ? 3.0f : 1.0f;
            ImU32 line_color = i == active_profile_idx ? 
                w::imcol32(ImGuiCol_ButtonActive) : 
                w::imcol32(ImGuiCol_Border);

            // Simple straight line for now - can be made curved later
            dl->AddLine(browser_point, profile_point, line_color, thickness);

            // Draw small dots at connection points
            dl->AddCircleFilled(browser_point, 2, col_dot);
            dl->AddCircleFilled(profile_point, 2, col_dot);
        }
    }

        //ImVec2 p0{min.x + 50, min.y + 50};
        //ImVec2 p1{min.x + 50, min.y + 100};
        //ImVec2 p2{min.x + 100, min.y + 250};
        //ImVec2 p3{min.x + 100, min.y + 300};

        //dl->AddCircleFilled(p0, 5, IM_COL32(255, 0, 0, 255), 32);
        //dl->AddCircleFilled(p1, 5, IM_COL32(255, 0, 0, 255), 32);
        //dl->AddCircleFilled(p2, 5, IM_COL32(255, 0, 0, 255), 32);
        //dl->AddCircleFilled(p3, 5, IM_COL32(255, 0, 0, 255), 32);

        //dl->AddBezierCubic(p0, p1, p2, p3, IM_COL32(0, 255, 0, 255), 2, 32);
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
