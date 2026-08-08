#pragma once
#include "imgui.h"

namespace UI {
    void ApplyModernTheme();
    
    // UI Helpers pour le nouveau design
    bool DrawNavTab(const char* label, bool active);
    bool DrawToolCard(const char* title, const char* desc, float width, bool active = false);
}
