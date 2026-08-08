#include "Theme.h"
#include "imgui_internal.h" 

namespace UI {
    void ApplyModernTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        
        style.WindowRounding    = 0.0f;
        style.ChildRounding     = 4.0f;
        style.FrameRounding     = 4.0f;
        style.PopupRounding     = 4.0f;
        
        style.WindowPadding     = ImVec2(0.0f, 0.0f); 
        style.FramePadding      = ImVec2(10.0f, 10.0f);
        style.ItemSpacing       = ImVec2(10.0f, 10.0f);
        
        style.WindowBorderSize  = 0.0f;
        style.ChildBorderSize   = 1.0f;
        style.FrameBorderSize   = 1.0f;

        
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_Border]                 = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        
        
        colors[ImGuiCol_Button]                 = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.18f, 0.12f, 0.28f, 1.00f);
        
        
        colors[ImGuiCol_Separator]              = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.65f, 0.35f, 1.00f, 0.50f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.65f, 0.35f, 1.00f, 1.00f);
        
        
        colors[ImGuiCol_Header]                 = ImVec4(0.15f, 0.10f, 0.22f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.20f, 0.12f, 0.30f, 1.00f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.65f, 0.35f, 1.00f, 0.30f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.12f, 0.08f, 0.18f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
        
        
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.20f, 0.12f, 0.30f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.22f, 0.60f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.65f, 0.35f, 1.00f, 1.00f);
        
        
        colors[ImGuiCol_FrameBg]                = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.15f, 0.10f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.12f, 0.30f, 1.00f);
    }

    bool DrawNavTab(const char* label, bool active) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        
        ImVec2 size = ImVec2(label_size.x + 20.0f, 40.0f); 

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        
        ImU32 text_col = ImGui::GetColorU32(active ? ImVec4(0.65f, 0.35f, 1.0f, 1.0f) : (hovered ? ImVec4(0.8f, 0.8f, 0.8f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
        
        
        window->DrawList->AddText(ImVec2(pos.x + 10.0f, pos.y + (size.y - label_size.y) / 2.0f), text_col, label);

        
        if (active) {
            window->DrawList->AddRectFilled(ImVec2(pos.x + 5.0f, pos.y + size.y - 2.0f), ImVec2(pos.x + size.x - 5.0f, pos.y + size.y), ImGui::GetColorU32(ImVec4(0.65f, 0.35f, 1.0f, 1.0f)));
        }

        return pressed;
    }

    bool DrawToolCard(const char* title, const char* desc, float width, bool active) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(title);
        
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImVec2(width, 120.0f); 
        
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        
        ImU32 bg_col;
        ImU32 border_col;
        
        if (active) {
            bg_col = ImGui::GetColorU32(hovered ? ImVec4(0.20f, 0.12f, 0.32f, 1.0f) : ImVec4(0.15f, 0.08f, 0.25f, 1.0f));
            border_col = ImGui::GetColorU32(ImVec4(0.65f, 0.35f, 1.0f, 1.0f));
        } else {
            bg_col = ImGui::GetColorU32(hovered ? ImVec4(0.12f, 0.12f, 0.12f, 1.0f) : ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
            border_col = ImGui::GetColorU32(hovered ? ImVec4(0.65f, 0.35f, 1.0f, 0.5f) : ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        }
        
        window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_col, 4.0f);
        window->DrawList->AddRect(bb.Min, bb.Max, border_col, 4.0f);

        
        window->DrawList->AddText(g.Font, g.FontSize * 1.1f, ImVec2(pos.x + 15.0f, pos.y + 15.0f), ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)), title);
        
        
        ImVec2 text_pos = ImVec2(pos.x + 15.0f, pos.y + 40.0f);
        window->DrawList->AddText(g.Font, g.FontSize * 0.9f, text_pos, ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), desc, NULL, size.x - 30.0f);

        return pressed;
    }
}
