#include "fluent2_theme.h"
#include "themes.h"
#include "win32/user.h"

using namespace std;

namespace bt::ui {
    std::string normalize_fluent2_theme_id(const std::string& theme_id) {
        if(theme_id == grey::themes::FollowOsThemeId || theme_id.empty()) {
            return grey::themes::FollowOsThemeId;
        }
        if(theme_id == "light" || theme_id == "microsoft") {
            return "light";
        }
        if(theme_id == "dark") {
            return "dark";
        }
        return "dark";
    }

    void apply_fluent2_theme(const std::string& theme_id, float scale) {
        const string normalized = normalize_fluent2_theme_id(theme_id);

        bool dark = true;
        if(normalized == grey::themes::FollowOsThemeId) {
            bool is_light = win32::user::is_app_light_theme();
            dark = !is_light;
        } else {
            dark = normalized == "dark";
        }

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* c = style.Colors;

        if(dark) {
            c[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            c[ImGuiCol_TextDisabled] = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
            c[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
            c[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
            c[ImGuiCol_PopupBg] = ImVec4(0.16f, 0.16f, 0.16f, 0.98f);
            c[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
            c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.29f, 0.43f, 1.00f);
            c[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
            c[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);
            c[ImGuiCol_ButtonActive] = ImVec4(0.17f, 0.29f, 0.43f, 1.00f);
            c[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.17f, 0.29f, 0.43f, 1.00f);
            c[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
            c[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
            c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            c[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
            c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
            c[ImGuiCol_CheckMark] = ImVec4(0.52f, 0.77f, 1.00f, 1.00f);
            c[ImGuiCol_SliderGrab] = ImVec4(0.52f, 0.77f, 1.00f, 1.00f);
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.61f, 0.82f, 1.00f, 1.00f);
            c[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.43f, 0.63f, 1.00f);
        } else {
            c[ImGuiCol_Text] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
            c[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
            c[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
            c[ImGuiCol_ChildBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
            c[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
            c[ImGuiCol_Border] = ImVec4(0.84f, 0.84f, 0.84f, 1.00f);
            c[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.97f, 1.00f, 1.00f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.89f, 0.94f, 0.99f, 1.00f);
            c[ImGuiCol_MenuBarBg] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
            c[ImGuiCol_Button] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.95f, 0.97f, 1.00f, 1.00f);
            c[ImGuiCol_ButtonActive] = ImVec4(0.89f, 0.94f, 0.99f, 1.00f);
            c[ImGuiCol_Header] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.95f, 0.97f, 1.00f, 1.00f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.89f, 0.94f, 0.99f, 1.00f);
            c[ImGuiCol_Tab] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
            c[ImGuiCol_TabHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            c[ImGuiCol_TabActive] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
            c[ImGuiCol_ScrollbarBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.79f, 0.79f, 0.79f, 1.00f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.72f, 0.72f, 0.72f, 1.00f);
            c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
            c[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.45f, 0.88f, 1.00f);
            c[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.45f, 0.88f, 1.00f);
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.40f, 0.80f, 1.00f);
            c[ImGuiCol_TextSelectedBg] = ImVec4(0.76f, 0.87f, 0.98f, 1.00f);
        }

        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_Separator] = c[ImGuiCol_Border];
        c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_Border];
        c[ImGuiCol_SeparatorActive] = c[ImGuiCol_Border];
        c[ImGuiCol_ResizeGrip] = c[ImGuiCol_Button];
        c[ImGuiCol_ResizeGripHovered] = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_ResizeGripActive] = c[ImGuiCol_ButtonActive];

        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisPrimary] = dark ? ImVec4(0.38f, 0.68f, 0.98f, 0.90f) : ImVec4(0.00f, 0.37f, 0.81f, 0.90f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisPrimaryHovered] = dark ? ImVec4(0.46f, 0.73f, 1.00f, 1.00f) : ImVec4(0.00f, 0.45f, 0.88f, 1.00f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisPrimaryActive] = dark ? ImVec4(0.31f, 0.61f, 0.93f, 1.00f) : ImVec4(0.00f, 0.32f, 0.75f, 1.00f);

        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisSecondary] = dark ? ImVec4(0.23f, 0.23f, 0.23f, 0.90f) : ImVec4(0.95f, 0.95f, 0.95f, 0.90f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisSecondaryHovered] = dark ? ImVec4(0.29f, 0.29f, 0.29f, 1.00f) : ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisSecondaryActive] = dark ? ImVec4(0.35f, 0.35f, 0.35f, 1.00f) : ImVec4(0.89f, 0.89f, 0.89f, 1.00f);

        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisSuccess] = dark ? ImVec4(0.42f, 0.78f, 0.47f, 0.90f) : ImVec4(0.06f, 0.52f, 0.15f, 0.90f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisSuccessHovered] = dark ? ImVec4(0.49f, 0.84f, 0.54f, 1.00f) : ImVec4(0.11f, 0.59f, 0.20f, 1.00f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisSuccessActive] = dark ? ImVec4(0.37f, 0.71f, 0.42f, 1.00f) : ImVec4(0.04f, 0.46f, 0.12f, 1.00f);

        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisError] = dark ? ImVec4(1.00f, 0.56f, 0.58f, 0.90f) : ImVec4(0.77f, 0.09f, 0.15f, 0.90f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisErrorHovered] = dark ? ImVec4(1.00f, 0.64f, 0.66f, 1.00f) : ImVec4(0.85f, 0.14f, 0.20f, 1.00f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisErrorActive] = dark ? ImVec4(0.95f, 0.49f, 0.51f, 1.00f) : ImVec4(0.69f, 0.07f, 0.12f, 1.00f);

        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisWarning] = dark ? ImVec4(1.00f, 0.76f, 0.34f, 0.90f) : ImVec4(0.69f, 0.40f, 0.00f, 0.90f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisWarningHovered] = dark ? ImVec4(1.00f, 0.81f, 0.45f, 1.00f) : ImVec4(0.76f, 0.46f, 0.05f, 1.00f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisWarningActive] = dark ? ImVec4(0.94f, 0.69f, 0.27f, 1.00f) : ImVec4(0.61f, 0.34f, 0.00f, 1.00f);

        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisInfo] = dark ? ImVec4(0.52f, 0.77f, 1.00f, 0.90f) : ImVec4(0.00f, 0.45f, 0.88f, 0.90f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisInfoHovered] = dark ? ImVec4(0.60f, 0.82f, 1.00f, 1.00f) : ImVec4(0.00f, 0.51f, 0.94f, 1.00f);
        grey::themes::GreyColors[grey::themes::GreyCol_EmphasisInfoActive] = dark ? ImVec4(0.44f, 0.71f, 0.96f, 1.00f) : ImVec4(0.00f, 0.40f, 0.80f, 1.00f);

        style.WindowPadding = ImVec2(12.0f * scale, 10.0f * scale);
        style.FramePadding = ImVec2(10.0f * scale, 6.0f * scale);
        style.CellPadding = ImVec2(8.0f * scale, 6.0f * scale);
        style.ItemSpacing = ImVec2(8.0f * scale, 6.0f * scale);
        style.ItemInnerSpacing = ImVec2(6.0f * scale, 6.0f * scale);
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;
        style.ScrollbarSize = 12.0f * scale;
        style.GrabMinSize = 10.0f * scale;
        style.WindowRounding = 12.0f * scale;
        style.ChildRounding = 12.0f * scale;
        style.FrameRounding = 10.0f * scale;
        style.PopupRounding = 12.0f * scale;
        style.ScrollbarRounding = 999.0f;
        style.GrabRounding = 10.0f * scale;
        style.TabRounding = 10.0f * scale;
    }
}
