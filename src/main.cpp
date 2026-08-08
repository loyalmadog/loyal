#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <chrono>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "UI/Theme.h"
#include "UI/TextureManager.h"
#include "Parsers/BAMParser.h"
#include "Parsers/PrefetchParser.h"
#include "Parsers/ServicesParser.h"
#include "Parsers/USBParser.h"
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")


static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


static bool g_IsDragging = false;
static POINT g_DragOffset;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    auto logger = spdlog::basic_logger_mt("SSTool", "sstool.log");
    spdlog::set_default_logger(logger);
    spdlog::info("SS Tool démarre.");

    
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"SSTool Class", nullptr };
    ::RegisterClassExW(&wc);
    
    int winWidth = 1100;
    int winHeight = 700;
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"SS Tool", WS_POPUP, 
                                (GetSystemMetrics(SM_CXSCREEN) - winWidth) / 2, 
                                (GetSystemMetrics(SM_CYSCREEN) - winHeight) / 2, 
                                winWidth, winHeight, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    
    ImFontConfig font_cfg;
    const char* fontPath = "fonts/Inter-Regular.ttf";
    FILE* f = fopen(fontPath, "rb");
    if (f) {
        fclose(f);
        io.Fonts->AddFontFromFileTTF(fontPath, 15.0f, &font_cfg);
    }
    
    if (io.Fonts->Fonts.empty()) {
        font_cfg.SizePixels = 15.0f;
        io.Fonts->AddFontDefault(&font_cfg);
    }

    UI::ApplyModernTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    UI::TextureManager::Initialize(g_pd3dDevice);

    ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    std::vector<Parsers::BAMEntry> bamEntries;
    bool bamLoaded = false;
    std::atomic<bool> isParsingBAM(false);
    const Parsers::BAMEntry* selectedBAMEntry = nullptr;

    std::vector<Parsers::ServiceEntry> servicesEntries;
    bool servicesLoaded = false;

    std::vector<Parsers::USBEntry> usbEntries;
    bool usbLoaded = false;
    std::atomic<bool> isParsingUSB(false);
    const Parsers::USBEntry* selectedUSBEntry = nullptr;
    int usbFilter = 0; 

    std::vector<Parsers::PrefetchEntry> prefetchEntries;
    bool prefetchLoaded = false;
    std::atomic<bool> isParsingPrefetch(false);
    static char prefetch_search[256] = "";
    const Parsers::PrefetchEntry* selectedPrefetchEntry = nullptr;

    std::atomic<bool> isDownloading(false);
    std::string downloadingToolName = "";

    int current_nav = 0; 

    
    static char bam_search[256] = "";
    bool bam_loaded = false;

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("MainViewport", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        
        
        float titlebarH = 40.0f;
        float btnW = 36.0f;
        ImVec2 winPos = ImGui::GetWindowPos();

        
        ImGui::GetWindowDrawList()->AddRectFilled(
            winPos,
            ImVec2(winPos.x + io.DisplaySize.x, winPos.y + titlebarH),
            ImGui::GetColorU32(ImVec4(0.07f, 0.07f, 0.07f, 1.0f))
        );

        
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(winPos.x + 15.0f, winPos.y + 12.0f),
            ImGui::GetColorU32(ImVec4(0.65f, 0.35f, 1.0f, 1.0f)), "SSTool");
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(winPos.x + 72.0f, winPos.y + 12.0f),
            ImGui::GetColorU32(ImVec4(0.38f, 0.38f, 0.38f, 1.0f)), "made by Loyal");

        
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("TitleBarDrag", ImVec2(io.DisplaySize.x - btnW, titlebarH));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (!g_IsDragging) {
                g_IsDragging = true;
                GetCursorPos(&g_DragOffset);
                RECT rect;
                GetWindowRect(hwnd, &rect);
                g_DragOffset.x -= rect.left;
                g_DragOffset.y -= rect.top;
            }
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, nullptr, pt.x - g_DragOffset.x, pt.y - g_DragOffset.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        } else {
            g_IsDragging = false;
        }

        
        float closeBtnSize = 24.0f;
        ImVec2 btnPos = ImVec2(winPos.x + io.DisplaySize.x - closeBtnSize - 12.0f, winPos.y + (titlebarH - closeBtnSize) / 2.0f);
        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - closeBtnSize - 12.0f, (titlebarH - closeBtnSize) / 2.0f));
        
        ImGui::InvisibleButton("CloseBtn", ImVec2(closeBtnSize, closeBtnSize));
        bool isHovered = ImGui::IsItemHovered();
        bool isActive = ImGui::IsItemActive();
        
        ImU32 btnColor = ImGui::GetColorU32(ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
        if (isActive) btnColor = ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        else if (isHovered) btnColor = ImGui::GetColorU32(ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        
        ImGui::GetWindowDrawList()->AddRectFilled(btnPos, ImVec2(btnPos.x + closeBtnSize, btnPos.y + closeBtnSize), btnColor, 4.0f);
        
        
        float crossSize = 10.0f;
        ImVec2 center = ImVec2(btnPos.x + closeBtnSize / 2.0f, btnPos.y + closeBtnSize / 2.0f);
        ImU32 crossColor = ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        
        
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(center.x - crossSize/2, center.y - crossSize/2), 
            ImVec2(center.x + crossSize/2, center.y + crossSize/2), crossColor, 1.5f);
        
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(center.x + crossSize/2, center.y - crossSize/2), 
            ImVec2(center.x - crossSize/2, center.y + crossSize/2), crossColor, 1.5f);
            
        if (isHovered && ImGui::IsMouseReleased(0)) {
            done = true;
        }

        
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(winPos.x, winPos.y + titlebarH),
            ImVec2(winPos.x + io.DisplaySize.x, winPos.y + titlebarH),
            ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
        );

        
        ImGui::SetCursorPos(ImVec2(10.0f, titlebarH + 2.0f));
        if (UI::DrawNavTab("Tools",    current_nav == 0)) current_nav = 0;
        ImGui::SameLine(0, 0);
        if (UI::DrawNavTab("Services", current_nav == 3)) current_nav = 3;
        ImGui::SameLine(0, 0);
        if (UI::DrawNavTab("Other",    current_nav == 4)) current_nav = 4;
        
        
        ImGui::SameLine(io.DisplaySize.x - 145.0f);
        ImGui::SetCursorPosY(titlebarH + 10.0f); 
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("Last Boot Time", ImVec2(120, 0))) {
            ImGui::OpenPopup("BootTimePopup");
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        
        float navBottom = titlebarH + 44.0f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(winPos.x, winPos.y + navBottom),
            ImVec2(winPos.x + io.DisplaySize.x, winPos.y + navBottom),
            ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
        );

        
        static int tools_sub = 0; 
        ImGui::SetCursorPos(ImVec2(0, navBottom + 1.0f));

        if (current_nav == 0) 
        {
            
            float btnAreaY = navBottom + 12.0f;
            ImGui::SetCursorPos(ImVec2(12.0f, btnAreaY));

            float availWidth = io.DisplaySize.x - 24.0f; 
            int columns = 5;
            float spacing = 10.0f;
            float cardWidth = (availWidth - (spacing * (columns - 1))) / columns;

            if (UI::DrawToolCard("BAM Parser", "Background Activity Moderator", cardWidth, tools_sub == 1)) {
                tools_sub = (tools_sub == 1) ? 0 : 1;
            }
            ImGui::SameLine(0, spacing);
            
            if (UI::DrawToolCard("Prefetch Viewer", "Application Execution History", cardWidth, tools_sub == 2)) {
                tools_sub = (tools_sub == 2) ? 0 : 2;
            }
            ImGui::SameLine(0, spacing);
            
            if (UI::DrawToolCard("Fileless", "Detect fileless via eventlog +\nmemdump", cardWidth)) {
                ShellExecuteA(NULL, "open", "cmd.exe", "/k powershell.exe -ExecutionPolicy Bypass -File fileless.ps1", NULL, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Detects fileless using powershell event logs and\nhas an option to analyze RAM, to detect possible\npe injects, shellcode, etc");
            }
            ImGui::SameLine(0, spacing);

            if (UI::DrawToolCard("USB History", "Historique des peripheriques\\nUSB connectes", cardWidth, tools_sub == 3)) {
                tools_sub = (tools_sub == 3) ? 0 : 3;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Affiche tous les peripheriques USB branches,\navec date premiere connexion, derniere connexion,\net derniere deconnexion");
            }

            ImGui::SameLine(0, spacing);

            if (UI::DrawToolCard("USN Journal", "Check USN Journal\nIntegrity", cardWidth)) {
                ShellExecuteA(NULL, "open", "cmd.exe", "/c powershell.exe -ExecutionPolicy Bypass -File usn_check.ps1", NULL, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Verify if the USN Journal ($UsnJrnl:$J)\nhas been deleted and recreated after boot");
            }

            
            float toolbarBottom = btnAreaY + 132.0f; 
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(winPos.x, winPos.y + toolbarBottom),
                ImVec2(winPos.x + io.DisplaySize.x, winPos.y + toolbarBottom),
                ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
            );
            if (tools_sub == 0) {
                
                float cy = toolbarBottom + (io.DisplaySize.y - toolbarBottom) / 2.0f - 10.0f;
                float cx = io.DisplaySize.x / 2.0f - 120.0f;
                ImGui::SetCursorPos(ImVec2(cx, cy));
                ImGui::TextDisabled("Select a tool above to get started.");
            }
            else if (tools_sub == 1) 
            {

                
                ImGui::SetCursorPos(ImVec2(12.0f, toolbarBottom + 8.0f));

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
                if (isParsingBAM) {
                    ImGui::Button("  Parsing...  ");
                } else {
                    if (ImGui::Button("  Parse Again  ") || !bamLoaded) {
                        isParsingBAM = true;
                        bamLoaded = false;
                        std::thread([&bamEntries, &bamLoaded, &isParsingBAM]() {
                            auto result = Parsers::BAMParser::Parse();
                            bamEntries = std::move(result);
                            bamLoaded = true;
                            isParsingBAM = false;
                        }).detach();
                    }
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine(0, 20.0f);
                static bool filter_not_signed  = false;
                static bool filter_in_instance = false;
                ImGui::Checkbox("Not Signed Only", &filter_not_signed);
                ImGui::SameLine(0, 20.0f);
                ImGui::Checkbox("In Instance Only", &filter_in_instance);

                float search_w = 280.0f;
                ImGui::SameLine(io.DisplaySize.x - search_w - 12.0f);
                ImGui::SetNextItemWidth(search_w);
                ImGui::InputTextWithHint("##bamsearch", "Search...", bam_search, sizeof(bam_search));
                float tableTop = toolbarBottom + 46.0f;
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(winPos.x, winPos.y + tableTop),
                    ImVec2(winPos.x + io.DisplaySize.x, winPos.y + tableTop),
                    ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
                );

                float tableH = io.DisplaySize.y - tableTop - 2.0f;
                if (!bamLoaded) {
                    float cx = io.DisplaySize.x / 2.0f - 140.0f;
                    float cy = ImGui::GetWindowSize().y / 2.0f - 10.0f;
                    ImGui::SetCursorPos(ImVec2(cx, cy));
                    ImGui::TextDisabled("Click \"Parse Again\" to load BAM entries.");
                } else {
                    std::string searchStr(bam_search);
                    bool openBAMPopup = false;
                    if (ImGui::BeginTable("BAMTable", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable,
                        ImVec2(0, tableH)))
                    {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Last Execution", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 160.0f, 0);
                        ImGui::TableSetupColumn("Executable Path", ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
                        ImGui::TableHeadersRow();

                        if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs()) {
                            if (sorts_specs->SpecsDirty) {
                                std::sort(bamEntries.begin(), bamEntries.end(), [sorts_specs](const Parsers::BAMEntry& a, const Parsers::BAMEntry& b) {
                                    const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[0];
                                    int delta = 0;
                                    switch (sort_spec->ColumnUserID) {
                                    case 0: delta = (a.Timestamp < b.Timestamp) ? -1 : (a.Timestamp > b.Timestamp) ? 1 : 0; break;
                                    case 1: delta = _stricmp(a.ExecutablePath.c_str(), b.ExecutablePath.c_str()); break;
                                    }
                                    return sort_spec->SortDirection == ImGuiSortDirection_Ascending ? delta < 0 : delta > 0;
                                });
                                sorts_specs->SpecsDirty = false;
                            }
                        }

                        for (const auto& entry : bamEntries) {
                            if (!searchStr.empty()) {
                                bool match = entry.ExecutablePath.find(searchStr) != std::string::npos ||
                                             entry.ExecutionTime.find(searchStr)  != std::string::npos;
                                if (!match) continue;
                            }
                            if (filter_not_signed  && entry.IsSigned)  continue;
                            if (filter_in_instance && !entry.IsRunning) continue;

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(entry.ExecutionTime.c_str());
                            ImGui::TableSetColumnIndex(1);

                            ID3D11ShaderResourceView* icon = UI::TextureManager::GetFileIcon(entry.ExecutablePath);
                            if (icon) { ImGui::Image((void*)icon, ImVec2(16, 16)); ImGui::SameLine(); }

                            std::string label = entry.ExecutablePath + "##bam" + std::to_string((unsigned long long)&entry);
                            bool selected = (selectedBAMEntry == &entry);
                            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                selectedBAMEntry = &entry;
                                openBAMPopup = true;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to copy path" : "Click for details (Hold Ctrl+Click to copy)");
                                if (ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(0))
                                    ImGui::SetClipboardText(entry.ExecutablePath.c_str());
                            }
                        }
                        ImGui::EndTable();
                    }

                    if (openBAMPopup) ImGui::OpenPopup("BAMDetailPopup");

                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
                    ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_Always);
                    if (ImGui::BeginPopupModal("BAMDetailPopup", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                        if (selectedBAMEntry) {
                            const auto& e = *selectedBAMEntry;
                            ImGui::TextColored(ImVec4(0.65f, 0.35f, 1.0f, 1.0f), e.ExecutablePath.c_str());
                            ImGui::Separator(); ImGui::Dummy(ImVec2(0, 4));
                            if (ImGui::BeginTabBar("BAMDetailTabs")) {
                                if (ImGui::BeginTabItem("Details")) {
                                    ImGui::Dummy(ImVec2(0, 6));
                                    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Executable");
                                    ImGui::Text("  Path:     %s", e.ExecutablePath.c_str());
                                    ImGui::Text("  Signed:   %s", e.IsSigned ? "Yes (Microsoft)" : "No");
                                    ImGui::Text("  Running:  %s", e.IsRunning ? "Yes" : "No");
                                    ImGui::EndTabItem();
                                }
                                if (ImGui::BeginTabItem("Execution History")) {
                                    ImGui::Dummy(ImVec2(0, 6));
                                    ImGui::Text("Run 1:  %s", e.ExecutionTime.c_str());
                                    ImGui::EndTabItem();
                                }
                                ImGui::EndTabBar();
                            }
                            ImGui::Dummy(ImVec2(0, 4)); ImGui::Separator(); ImGui::Dummy(ImVec2(0, 4));
                            float bw = 100.0f;
                            ImGui::SetCursorPosX(210.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8.0f);
                            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
                            if (ImGui::Button("Close", ImVec2(bw, 28))) { ImGui::CloseCurrentPopup(); selectedBAMEntry = nullptr; }
                            ImGui::PopStyleColor(3);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor();
                }
            
            
            }
            else if (tools_sub == 2) 
            {

                
                ImGui::SetCursorPos(ImVec2(12.0f, toolbarBottom + 8.0f));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
            if (isParsingPrefetch) {
                ImGui::Button("  Parsing...  ");
            } else {
                if (ImGui::Button("  Parse Again  ") || !prefetchLoaded) {
                    isParsingPrefetch = true;
                    prefetchLoaded = false;
                    std::thread([&prefetchEntries, &prefetchLoaded, &isParsingPrefetch]() {
                        auto result = Parsers::PrefetchParser::Parse();
                        prefetchEntries = std::move(result);
                        prefetchLoaded = true;
                        isParsingPrefetch = false;
                    }).detach();
                }
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0, 20.0f);

            
            static bool pf_filter_in_instance = false;
            static bool pf_filter_not_found = false;

            
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.9f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.12f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.18f, 0.12f, 1.0f));
            ImGui::Checkbox("In Instance Only", &pf_filter_in_instance);
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Show only entries executed since the last PC boot");

            ImGui::SameLine(0, 20.0f);

            
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
            ImGui::Checkbox("Show Not Found", &pf_filter_not_found);
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Show only entries where the executable could not be located on disk");

            
            float search_w = 280.0f;
            ImGui::SameLine(io.DisplaySize.x - search_w - 12.0f);
            ImGui::SetNextItemWidth(search_w);
            ImGui::InputTextWithHint("##pfsearch", "Search...", prefetch_search, sizeof(prefetch_search));

            
            float tableTop = toolbarBottom + 46.0f;
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(winPos.x, winPos.y + tableTop),
                    ImVec2(winPos.x + io.DisplaySize.x, winPos.y + tableTop),
                    ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
                );

            
            bool pf_show_in_instance_only = pf_filter_in_instance;
            bool pf_show_not_found_only = pf_filter_not_found;

            
            uint64_t bootTimestamp = 0;
            if (pf_show_in_instance_only) {
                ULONGLONG uptimeMs = GetTickCount64();
                FILETIME nowFt; GetSystemTimeAsFileTime(&nowFt);
                ULARGE_INTEGER now; now.LowPart = nowFt.dwLowDateTime; now.HighPart = nowFt.dwHighDateTime;
                bootTimestamp = now.QuadPart - (uptimeMs * 10000ULL);
            }

            
            ImGui::SetCursorPos(ImVec2(0, tableTop + 2.0f));
            float tableH = io.DisplaySize.y - tableTop - 2.0f;

            if (!prefetchLoaded) {
                float cx = io.DisplaySize.x / 2.0f - 140.0f;
                float cy = ImGui::GetWindowSize().y / 2.0f - 10.0f;
                ImGui::SetCursorPos(ImVec2(cx, cy));
                ImGui::TextDisabled("Click \"Parse Again\" to load Prefetch entries.");
            } else {
                std::string searchStr(prefetch_search);
                bool openPrefetchPopup = false;
                if (ImGui::BeginTable("PrefetchTable", 2,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_Sortable,
                    ImVec2(0, tableH)))
                {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Last Execution", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 160.0f, 0);
                    ImGui::TableSetupColumn("Executable",     ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
                    ImGui::TableHeadersRow();

                    
                    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
                        if (specs->SpecsDirty) {
                            std::sort(prefetchEntries.begin(), prefetchEntries.end(),
                                [specs](const Parsers::PrefetchEntry& a, const Parsers::PrefetchEntry& b) {
                                    const ImGuiTableColumnSortSpecs* s = &specs->Specs[0];
                                    int delta = 0;
                                    switch (s->ColumnUserID) {
                                    case 0: delta = (a.LastRunTimestamp < b.LastRunTimestamp) ? -1 : (a.LastRunTimestamp > b.LastRunTimestamp) ? 1 : 0; break;
                                    case 1: delta = _stricmp(a.ExecutableName.c_str(), b.ExecutableName.c_str()); break;
                                    }
                                    return s->SortDirection == ImGuiSortDirection_Ascending ? delta < 0 : delta > 0;
                                });
                            specs->SpecsDirty = false;
                        }
                    }

                    for (size_t ei = 0; ei < prefetchEntries.size(); ++ei) {
                        const auto& entry = prefetchEntries[ei];
                        if (!searchStr.empty()) {
                            bool match = entry.ExecutableName.find(searchStr) != std::string::npos ||
                                         entry.LastRunTime.find(searchStr)    != std::string::npos;
                            if (!match) continue;
                        }
                        
                        if (pf_show_in_instance_only && entry.LastRunTimestamp < bootTimestamp) continue;
                        if (pf_show_not_found_only && entry.ExistsOnDisk) continue;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(entry.LastRunTime.c_str());

                        ImGui::TableSetColumnIndex(1);

                        
                        std::string selId = entry.ExecutableName + "##pf" + std::to_string(ei);
                        bool selected = (selectedPrefetchEntry == &entry);

                        ImVec4 textColor;
                        if (!entry.ExistsOnDisk)     textColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                        else if (!entry.IsSigned)    textColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                        else                         textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

                        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                        if (ImGui::Selectable(selId.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                            selectedPrefetchEntry = &prefetchEntries[ei];
                            openPrefetchPopup = true;
                        }
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Click for details");
                    }
                    ImGui::EndTable();

                    if (openPrefetchPopup) {
                        ImGui::OpenPopup("PrefetchDetailPopup");
                    }

                    
                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
                    ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_Always);
                    if (ImGui::BeginPopupModal("PrefetchDetailPopup", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                        if (selectedPrefetchEntry) {
                            const auto& e = *selectedPrefetchEntry;

                            
                            ImGui::TextColored(ImVec4(0.65f, 0.35f, 1.0f, 1.0f), e.ExecutableName.c_str());
                            ImGui::Separator();
                            ImGui::Dummy(ImVec2(0, 4));

                            if (ImGui::BeginTabBar("PrefetchDetailTabs")) {

                                
                                if (ImGui::BeginTabItem("Details")) {
                                    ImGui::Dummy(ImVec2(0, 6));

                                    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Prefetch File");
                                    ImGui::Text("  File:     %s", e.PrefetchFile.c_str());
                                    ImGui::Text("  Size:     %s", e.PfSize.c_str());
                                    ImGui::Text("  Created:  %s", e.PfCreated.c_str());
                                    ImGui::Text("  Modified: %s", e.PfModified.c_str());

                                    ImGui::Dummy(ImVec2(0, 8));
                                    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Executable");
                                    if (e.ExistsOnDisk) {
                                        ImGui::Text("  Path:     %s", e.ResolvedPath.c_str());
                                        ImGui::Text("  Size:     %s", e.ExeSize.c_str());
                                        ImGui::Text("  Created:  %s", e.ExeCreated.c_str());
                                        ImGui::Text("  Modified: %s", e.ExeModified.c_str());
                                        ImGui::Text("  Signed:   %s", e.IsSigned ? "Yes" : "No");
                                    } else {
                                        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "  Not found on disk");
                                    }
                                    ImGui::EndTabItem();
                                }

                                
                                if (ImGui::BeginTabItem("Execution History")) {
                                    ImGui::Dummy(ImVec2(0, 6));
                                    if (e.RunHistory.empty()) {
                                        ImGui::TextDisabled("No run history available.");
                                    } else {
                                        for (size_t ri = 0; ri < e.RunHistory.size(); ++ri) {
                                            ImGui::Text("Run %zu:  %s", ri + 1, e.RunHistory[ri].c_str());
                                        }
                                    }
                                    ImGui::EndTabItem();
                                }

                                ImGui::EndTabBar();
                            }

                            ImGui::Dummy(ImVec2(0, 4));
                            ImGui::Separator();
                            ImGui::Dummy(ImVec2(0, 4));
                            float btnW = 100.0f;
                            ImGui::SetCursorPosX(210.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8.0f);
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
                            if (ImGui::Button("Close", ImVec2(btnW, 28))) {
                                ImGui::CloseCurrentPopup();
                                selectedPrefetchEntry = nullptr;
                            }
                            ImGui::PopStyleColor(3);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor();
                }
            }
            
            
            }
            else if (tools_sub == 3) 
            {

                ImGui::SetCursorPos(ImVec2(12.0f, toolbarBottom + 8.0f));

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
                if (isParsingUSB) {
                    ImGui::Button("  Scanning...  ");
                } else {
                    if (ImGui::Button("  Refresh  ") || !usbLoaded) {
                        isParsingUSB = true;
                        usbLoaded = false;
                        std::thread([&usbEntries, &usbLoaded, &isParsingUSB]() {
                            usbEntries   = Parsers::USBParser::Parse();
                            usbLoaded    = true;
                            isParsingUSB = false;
                        }).detach();
                    }
                }
                ImGui::PopStyleColor(3);

                
                ImGui::SameLine(0, 14);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                {
                    auto FilterBtn = [&](const char* label, int idx) {
                        bool active = (usbFilter == idx);
                        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.25f, 0.80f, 1.0f));
                        else        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.10f, 0.22f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
                        if (ImGui::Button(label)) usbFilter = idx;
                        ImGui::PopStyleColor(3);
                    };
                    FilterBtn("All", 0);
                    ImGui::SameLine(0, 4);
                    FilterBtn("Connected", 1);
                    ImGui::SameLine(0, 4);
                    FilterBtn("Disconnected", 2);
                }
                ImGui::PopStyleVar();

                
                if (usbLoaded && !usbEntries.empty()) {
                    int cnt = 0;
                    for (auto& e : usbEntries) {
                        if (usbFilter == 0) cnt++;
                        else if (usbFilter == 1 && e.IsConnected) cnt++;
                        else if (usbFilter == 2 && !e.IsConnected) cnt++;
                    }
                    ImGui::SameLine(0, 18);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                    ImGui::TextDisabled("%d device(s)", cnt);
                }
                float tableTop = toolbarBottom + 46.0f;
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(winPos.x, winPos.y + tableTop),
                    ImVec2(winPos.x + io.DisplaySize.x, winPos.y + tableTop),
                    ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f))
                );

                float tableH = io.DisplaySize.y - tableTop - 2.0f;

                if (!usbLoaded) {
                    float cx = io.DisplaySize.x / 2.0f - 120.0f;
                    float cy = ImGui::GetWindowSize().y / 2.0f - 10.0f;
                    ImGui::SetCursorPos(ImVec2(cx, cy));
                    ImGui::TextDisabled("Click \"Refresh\" to load USB history.");
                } else if (usbEntries.empty()) {
                    float cx = io.DisplaySize.x / 2.0f - 120.0f;
                    float cy = ImGui::GetWindowSize().y / 2.0f - 10.0f;
                    ImGui::SetCursorPos(ImVec2(cx, cy));
                    ImGui::TextDisabled("Aucun peripherique USB trouve dans le registre.");
                } else {
                    bool openUSBPopup = false;
                    if (ImGui::BeginTable("USBTable", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable,
                        ImVec2(0, tableH)))
                    {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Device Name",    ImGuiTableColumnFlags_WidthStretch,                              0.0f, 0);
                        ImGui::TableSetupColumn("Vendor Name",    ImGuiTableColumnFlags_WidthStretch,                              0.0f, 1);
                        ImGui::TableSetupColumn("VID/PID",        ImGuiTableColumnFlags_WidthFixed,                              100.0f, 2);
                        ImGui::TableSetupColumn("Last Connect",   ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 150.0f, 3);
                        ImGui::TableSetupColumn("Last Removal",   ImGuiTableColumnFlags_WidthFixed,                              150.0f, 4);
                        ImGui::TableSetupColumn("Capabilities",   ImGuiTableColumnFlags_WidthStretch,                              0.0f, 5);
                        ImGui::TableSetupColumn("Connected",      ImGuiTableColumnFlags_WidthFixed,                               80.0f, 6);
                        ImGui::TableHeadersRow();

                        
                        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
                            if (specs->SpecsDirty) {
                                std::sort(usbEntries.begin(), usbEntries.end(),
                                    [specs](const Parsers::USBEntry& a, const Parsers::USBEntry& b) {
                                        const ImGuiTableColumnSortSpecs* s = &specs->Specs[0];
                                        int delta = 0;
                                        switch (s->ColumnUserID) {
                                        case 0: delta = _stricmp(a.FriendlyName.c_str(), b.FriendlyName.c_str()); break;
                                        case 1: delta = _stricmp(a.DeviceDesc.c_str(),   b.DeviceDesc.c_str());   break;
                                        case 3: delta = (a.LastTimestamp  < b.LastTimestamp)  ? -1 : (a.LastTimestamp  > b.LastTimestamp)  ? 1 : 0; break;
                                        case 4: delta = (a.RemoveTimestamp < b.RemoveTimestamp) ? -1 : (a.RemoveTimestamp > b.RemoveTimestamp) ? 1 : 0; break;
                                        case 6: delta = (int)a.IsConnected - (int)b.IsConnected; break;
                                        }
                                        return s->SortDirection == ImGuiSortDirection_Ascending ? delta < 0 : delta > 0;
                                    });
                                specs->SpecsDirty = false;
                            }
                        }

                        for (size_t ui = 0; ui < usbEntries.size(); ++ui) {
                            const auto& e = usbEntries[ui];
                            
                            if (usbFilter == 1 && !e.IsConnected) continue;
                            if (usbFilter == 2 && e.IsConnected)  continue;
                            ImGui::TableNextRow();

                            
                            ImGui::TableSetColumnIndex(0);
                            std::string selId = (e.FriendlyName.empty() ? e.DeviceClass : e.FriendlyName) + "##usb" + std::to_string(ui);
                            bool selected = (selectedUSBEntry == &e);
                            if (ImGui::Selectable(selId.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                selectedUSBEntry = &usbEntries[ui];
                                openUSBPopup = true;
                            }

                            
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(e.DeviceDesc.empty() ? e.Manufacturer.c_str() : e.DeviceDesc.c_str());

                            
                            ImGui::TableSetColumnIndex(2);
                            if (!e.VID.empty() || !e.PID.empty()) {
                                char vidpid[32];
                                sprintf_s(vidpid, "%s / %s", e.VID.c_str(), e.PID.c_str());
                                ImGui::TextColored(ImVec4(0.40f, 0.75f, 1.0f, 1.0f), "%s", vidpid);
                            } else {
                                ImGui::TextDisabled("-");
                            }

                            
                            ImGui::TableSetColumnIndex(3);
                            ImGui::TextColored(ImVec4(0.40f, 0.75f, 1.0f, 1.0f), "%s", e.LastConnected.empty() ? "-" : e.LastConnected.c_str());

                            
                            ImGui::TableSetColumnIndex(4);
                            ImGui::TextUnformatted(e.LastRemoved.empty() ? "" : e.LastRemoved.c_str());

                            
                            ImGui::TableSetColumnIndex(5);
                            ImGui::TextUnformatted(e.Capabilities.c_str());

                            
                            ImGui::TableSetColumnIndex(6);
                            if (e.IsConnected)
                                ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.2f, 1.0f), "Yes");
                            else
                                ImGui::TextColored(ImVec4(0.85f, 0.2f, 0.2f, 1.0f), "No");
                        }
                        ImGui::EndTable();
                    }

                    if (openUSBPopup) ImGui::OpenPopup("USBDetailPopup");

                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
                    ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_Always);
                    if (ImGui::BeginPopupModal("USBDetailPopup", NULL,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
                    {
                        if (selectedUSBEntry) {
                            const auto& e = *selectedUSBEntry;
                            ImGui::TextColored(ImVec4(0.65f, 0.35f, 1.0f, 1.0f),
                                e.FriendlyName.empty() ? e.DeviceClass.c_str() : e.FriendlyName.c_str());
                            ImGui::Separator(); ImGui::Dummy(ImVec2(0, 6));

                            auto Row = [](const char* label, const char* value) {
                                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), label);
                                ImGui::SameLine(160.0f);
                                ImGui::TextUnformatted(value && *value ? value : "-");
                            };

                            Row("Peripherique :",         e.FriendlyName.c_str());
                            Row("Fabricant :",            e.Manufacturer.c_str());
                            {
                                std::string vidpid = e.VID + " / " + e.PID;
                                Row("VID/PID :",          vidpid.c_str());
                            }
                            Row("Numero de serie :",      e.SerialNumber.c_str());
                            Row("Classe :",               e.DeviceClass.c_str());
                            Row("Capabilities :",         e.Capabilities.c_str());
                            ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Connecte :");
                            ImGui::SameLine(160.0f);
                            if (e.IsConnected)
                                ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.2f, 1.0f), "Yes");
                            else
                                ImGui::TextColored(ImVec4(0.85f, 0.2f, 0.2f, 1.0f), "No");
                            ImGui::Dummy(ImVec2(0, 6));
                            ImGui::Separator(); ImGui::Dummy(ImVec2(0, 6));
                            Row("Premiere connexion :",   e.FirstConnected.c_str());
                            Row("Derniere connexion :",   e.LastConnected.c_str());
                            Row("Derniere deconnexion :", e.LastRemoved.c_str());

                            ImGui::Dummy(ImVec2(0, 4));
                            ImGui::Separator(); ImGui::Dummy(ImVec2(0, 4));
                            float bw = 100.0f;
                            ImGui::SetCursorPosX(230.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8.0f);
                            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
                            if (ImGui::Button("Close", ImVec2(bw, 28))) {
                                ImGui::CloseCurrentPopup();
                                selectedUSBEntry = nullptr;
                            }
                            ImGui::PopStyleColor(3);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor();
                }
            
            
            }

                    }
        else if (current_nav == 3) 
        {
            ImGui::SetCursorPos(ImVec2(12.0f, navBottom + 8.0f));
            
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
            if (ImGui::Button("  Refresh Services  ") || !servicesLoaded) {
                servicesEntries = Parsers::ServicesParser::Parse();
                servicesLoaded = true;
            }
            ImGui::PopStyleColor(3);
            
            ImGui::SetCursorPos(ImVec2(12.0f, navBottom + 45.0f));
            float tableH = io.DisplaySize.y - (navBottom + 55.0f);
            
            if (ImGui::BeginTable("ServicesTable", 3,
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingStretchProp,
                ImVec2(0, tableH)))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Service Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Display Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Status",       ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                for (const auto& entry : servicesEntries) {
                    ImGui::TableNextRow();
                    
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(entry.Name.c_str());
                    
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(entry.DisplayName.c_str());
                    
                    ImGui::TableSetColumnIndex(2);
                    if (entry.Status == "Running") {
                        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), entry.Status.c_str());
                    } else if (entry.Status == "Stopped") {
                        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), entry.Status.c_str());
                    } else if (entry.Status == "Not Found") {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), entry.Status.c_str());
                    } else {
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), entry.Status.c_str()); 
                    }
                }
                ImGui::EndTable();
            }
        }
        else if (current_nav == 4) 
        {
            ImGui::SetCursorPos(ImVec2(12.0f, navBottom + 12.0f));
            
            
            float availWidth = io.DisplaySize.x - 24.0f; 
            int columns = 5;
            float spacing = 10.0f;
            float cardWidth = (availWidth - (spacing * (columns - 1))) / columns;
            
            
            if (UI::DrawToolCard("Everything", "Everything search engine", cardWidth)) {
                const char* path1 = "C:\\Program Files\\Everything\\Everything.exe";
                const char* path2 = "C:\\Program Files (x86)\\Everything\\Everything.exe";
                const char* localPath = "Everything.exe";
                const char* installer = "Everything_Setup.exe";
                
                if (GetFileAttributesA(path1) != INVALID_FILE_ATTRIBUTES) {
                    ShellExecuteA(NULL, "open", path1, NULL, NULL, SW_SHOWNORMAL);
                } else if (GetFileAttributesA(path2) != INVALID_FILE_ATTRIBUTES) {
                    ShellExecuteA(NULL, "open", path2, NULL, NULL, SW_SHOWNORMAL);
                } else if (GetFileAttributesA(localPath) != INVALID_FILE_ATTRIBUTES) {
                    ShellExecuteA(NULL, "open", localPath, NULL, NULL, SW_SHOWNORMAL);
                } else if (GetFileAttributesA(installer) != INVALID_FILE_ATTRIBUTES) {
                    ShellExecuteA(NULL, "open", installer, NULL, NULL, SW_SHOWNORMAL);
                } else {
                    isDownloading = true;
                    downloadingToolName = "Everything";
                    std::thread([&isDownloading]() {
                        URLDownloadToFileA(NULL, "https://www.voidtools.com/Everything-1.4.1.1024.x64-Setup.exe", "Everything_Setup.exe", 0, NULL);
                        ShellExecuteA(NULL, "open", "Everything_Setup.exe", NULL, NULL, SW_SHOWNORMAL);
                        isDownloading = false;
                    }).detach();
                }
            }
            
            ImGui::SameLine(0, spacing);
            
            
            if (UI::DrawToolCard("JournalTrace", "Parses NTFS journal entries", cardWidth)) {
                const char* localPath = "JournalTrace.exe";
                
                if (GetFileAttributesA(localPath) != INVALID_FILE_ATTRIBUTES) {
                    ShellExecuteA(NULL, "open", localPath, NULL, NULL, SW_SHOWNORMAL);
                } else {
                    isDownloading = true;
                    downloadingToolName = "JournalTrace";
                    std::thread([&isDownloading]() {
                        URLDownloadToFileA(NULL, "https://github.com/spokwn/JournalTrace/releases/latest/download/JournalTrace.exe", "JournalTrace.exe", 0, NULL);
                        ShellExecuteA(NULL, "open", "JournalTrace.exe", NULL, NULL, SW_SHOWNORMAL);
                        isDownloading = false;
                    }).detach();
                }
            }
        }
        else
        {
            
            float cx = ImGui::GetWindowSize().x / 2.0f - 100.0f;
            float cy = io.DisplaySize.y / 2.0f;
            ImGui::SetCursorPos(ImVec2(cx, cy));
            ImGui::TextDisabled("Coming soon...");
        }

        
        if (isDownloading && !ImGui::IsPopupOpen("DownloadingPopup")) {
            ImGui::OpenPopup("DownloadingPopup");
        }

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));
        if (ImGui::BeginPopupModal("DownloadingPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar)) {
            ImGui::TextColored(ImVec4(0.65f, 0.35f, 1.0f, 1.0f), "Downloading %s...", downloadingToolName.c_str());
            ImGui::Dummy(ImVec2(0, 10.0f));
            ImGui::TextUnformatted("Please wait while the tool is being downloaded.");
            
            if (!isDownloading) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));
        if (ImGui::BeginPopupModal("BootTimePopup", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar)) {
            
            ImGui::TextColored(ImVec4(0.65f, 0.35f, 1.0f, 1.0f), "System Last Boot Time");
            ImGui::Dummy(ImVec2(0, 4.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 10.0f));
            
            
            ULONGLONG uptimeMs = GetTickCount64();
            FILETIME nowFt; GetSystemTimeAsFileTime(&nowFt);
            ULARGE_INTEGER now; now.LowPart = nowFt.dwLowDateTime; now.HighPart = nowFt.dwHighDateTime;
            now.QuadPart -= (uptimeMs * 10000ULL);
            FILETIME bootFt; bootFt.dwLowDateTime = now.LowPart; bootFt.dwHighDateTime = now.HighPart;
            SYSTEMTIME stUTC, stLocal;
            FileTimeToSystemTime(&bootFt, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
            char buf[128];
            sprintf_s(buf, "%02d/%02d/%04d at %02d:%02d:%02d", stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
            
            ImGui::TextUnformatted("Your PC was last booted on:");
            ImGui::Dummy(ImVec2(0, 4.0f));
            
            
            float textWidth = ImGui::CalcTextSize(buf).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), buf);
            
            ImGui::Dummy(ImVec2(0, 14.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 10.0f));
            
            
            float btnWidth = 100.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnWidth) * 0.5f);
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.18f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.35f, 1.00f, 1.0f));
            if (ImGui::Button("Close", ImVec2(btnWidth, 28))) { 
                ImGui::CloseCurrentPopup(); 
            }
            ImGui::PopStyleColor(3);
            
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        

        ImGui::End();

        
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); 
    }

    UI::TextureManager::Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
