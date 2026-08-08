#pragma once
#include "imgui.h"

namespace UI {
    void ApplyModernTheme();
    
    
    bool DrawNavTab(const char* label, bool active);
    bool DrawToolCard(const char* title, const char* desc, float width, bool active = false);
}
