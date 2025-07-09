#pragma once
#include <vector>
#include <memory>
#include "grey.h"
#include "../browser.h"

namespace bt::ui {

    class picker_app {
    public:

        struct connection_box {
            ImVec2 min;
            ImVec2 max;
        };

        struct action_menu_item {
            std::string id;
            std::string icon;
            std::string tooltip;
            float x{0};
            float y{0};
            float angle{0};
            float angle_final{0};
        };


        picker_app(const std::string& url);
        ~picker_app();

        std::shared_ptr<bt::browser_instance> run();

        static bool is_hotkey_down();

    private:

        const float BrowserSquareSize = 60.0f;
        const float BrowserSquarePadding = 10.0f;
        const float InactiveBrowserSquarePadding = 12.0f;
        const float ProfileSquareSize = 40.0f;
        const float ProfileSquarePadding = 5.0f;
        const float InactiveProfileSquarePadding = 7.0f;
        const float ActionSquareSize = 20.0f;
        const float WindowMinWidth = BrowserSquareSize * 6;
        const float WindowHeight = BrowserSquareSize + ProfileSquareSize + 80;
        float wnd_width;
        bool has_profile_bar{false};
        int keyboard_selection_idx{-1};

        const float InactiveAlpha = 0.8;

        std::string url;
        std::string title;
        std::unique_ptr<grey::app> app;
        bool is_open{true};

        // calculated
        std::vector<bt::browser> browsers;
        int active_browser_idx{-1};
        int active_profile_idx{-1};
        connection_box active_browser_cb;        // active browser coordinates
        std::vector<connection_box> profiles_cb; // active profile coordinates
        float browser_bar_left_pad{0};

        bool action_menu_hovered{false};
        std::vector<action_menu_item> action_menu_items{
            action_menu_item{"copy", ICON_MD_CONTENT_COPY, "Copy to clipboard & close"},
            action_menu_item{"email", ICON_MD_EMAIL, "Email link"}
        };

        grey::widgets::window wnd_main;

        std::shared_ptr<bt::browser_instance> decision;

        bool run_frame();
        void render_action_menu(float x, float y);
        void render_browser_bar();
        void render_profile_bar();
        void render_connection_box();

        /**
         * @brief Invoked on user's final decision
         * @param decision 
         */
        void make_decision(std::shared_ptr<bt::browser_instance> decision);

        void menu_item_clicked(const std::string& id);
    };
}
