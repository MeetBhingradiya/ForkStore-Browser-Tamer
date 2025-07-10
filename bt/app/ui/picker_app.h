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

        // Enhanced responsive design constants
        const float MinWindowWidth = 480.0f;
        const float MinWindowHeight = 360.0f;
        const float MaxCardSize = 180.0f;
        const float MinCardSize = 120.0f;
        const float CardSpacing = 24.0f;
        const float GridPadding = 32.0f;
        
        // Modern card styling constants
        const float CardRounding = 20.0f;
        const float IconSize = 72.0f;
        const float BadgeSize = 28.0f;
        const float ShadowOffset = 8.0f;
        const float BorderWidth = 2.0f;
        
        // Responsive grid settings
        mutable int ProfilesPerRow = 4;
        const int MaxProfilesPerRow = 5;
        const int MinProfilesPerRow = 2;
        
        // Animation and interaction constants
        const float HoverScaleFactor = 1.05f;
        const float AnimationDuration = 0.3f;
        const float GlassOpacity = 0.15f;
        const float HoverOpacity = 0.25f;
        const float SelectedOpacity = 0.35f;
        
        // Legacy constants (kept for compatibility)
        const float BrowserSquareSize = 60.0f;
        const float BrowserSquarePadding = 10.0f;
        const float InactiveBrowserSquarePadding = 12.0f;
        const float ProfileSquareSize = 40.0f;
        const float ProfileSquarePadding = 5.0f;
        const float InactiveProfileSquarePadding = 7.0f;
        const float ActionSquareSize = 20.0f;
        const float InactiveAlpha = 0.8f;
        
        // Window management
        float wnd_width;
        const float WindowHeight = BrowserSquareSize + ProfileSquareSize + 80;
        bool has_profile_bar{false};
        int keyboard_selection_idx{-1};

        // Application state
        std::string url;
        std::string title;
        std::unique_ptr<grey::app> app;
        bool is_open{true};

        // Profile card system
        struct ProfileCard {
            std::shared_ptr<bt::browser_instance> instance;
            std::string browser_icon_path;
            std::string profile_name;
            std::string profile_icon_path;
            bool is_incognito{false};
            
            // Enhanced card properties for animations
            float hover_progress{0.0f};
            float scale_factor{1.0f};
            bool is_animating{false};
        };
        
        std::vector<ProfileCard> profile_cards;
        int active_profile_idx{-1};
        std::vector<connection_box> profiles_cb;

        // Action menu system
        bool action_menu_hovered{false};
        std::vector<action_menu_item> action_menu_items{
            action_menu_item{"copy", ICON_MD_CONTENT_COPY, "Copy to clipboard & close"},
            action_menu_item{"email", ICON_MD_EMAIL, "Email link"}
        };

        // UI components
        grey::widgets::window wnd_main;
        std::shared_ptr<bt::browser_instance> decision;

        // Core rendering methods
        bool run_frame();
        void render_glass_background();
        void render_url_input();
        void render_profile_grid(float win_width);
        void render_profile_card(const ProfileCard& card, int idx, float x, float y, 
                                float card_size, float icon_size, float badge_size);
        void render_action_menu(float x, float y);
        void render_connection_box();

        // Responsive design methods
        void calculate_responsive_layout();
        int calculate_optimal_columns(int profile_count, float available_width);
        float calculate_card_size(float available_width, int columns);
        
        // Animation and interaction methods
        void update_card_animations(float delta_time);
        void handle_card_hover_effects(int card_index, bool is_hovered);
        
        // Decision handling
        void make_decision(std::shared_ptr<bt::browser_instance> decision);
        void menu_item_clicked(const std::string& id);
        
        // Utility methods
        ImU32 get_glass_color(float opacity) const;
        ImU32 get_accent_color(float opacity) const;
        void apply_glass_style_colors();
        void restore_default_style_colors();
    };
}