#pragma once
#include <vector>
#include <memory>
#include "grey.h"
#include "../browser.h"

namespace bt::ui {

    struct action_menu_item {
        std::string id;
        std::string icon;
        std::string tooltip;
        float angle{0};
        float angle_final{0};
        float x{0}, y{0};
    };

    class picker_app {
    private:
        // Chrome-style layout constants
        static constexpr int WindowMinWidth = 800;
        static constexpr int WindowHeight = 600;
        static constexpr int ProfileCardWidth = 180;
        static constexpr int ProfileCardHeight = 220;
        static constexpr int ProfileCardPadding = 16;
        static constexpr int ProfileCardRounding = 12;
        static constexpr int ProfileGridColumns = 4;
        static constexpr int ProfileHeaderHeight = 120;
        static constexpr int ProfileHeaderPadding = 20;
        static constexpr int ProfileFooterHeight = 60;
        static constexpr int ProfileFooterPadding = 20;

        std::string url;
        std::string title;
        std::shared_ptr<grey::app> app;
        grey::widgets::window wnd_main;
        bool is_open{true};

        std::vector<bt::browser> browsers;
        std::shared_ptr<bt::browser_instance> decision;

        int keyboard_selection_idx{-1};
        bool action_menu_hovered{false};
        
        float wnd_width{WindowMinWidth};
        float wnd_height{WindowHeight};

        std::vector<action_menu_item> action_menu_items{
            {"copy", ICON_MD_CONTENT_COPY, "Copy URL"},
            {"email", ICON_MD_EMAIL, "Email URL"}
        };

    public:
        picker_app(const std::string& url);
        ~picker_app();

        std::shared_ptr<bt::browser_instance> run();
        bool is_hotkey_down();

    private:
        bool run_frame();
        void render_header();
        void render_profile_grid();
        void render_profile_card(bt::browser& browser, std::shared_ptr<bt::browser_instance> profile, float x, float y, int index);
        void render_footer();
        void render_action_menu(float x, float y);
        void make_decision(std::shared_ptr<bt::browser_instance> decision);
        void menu_item_clicked(const std::string& id);
    };
}
