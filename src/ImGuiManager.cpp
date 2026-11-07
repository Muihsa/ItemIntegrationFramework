#include "pch.h"
#include "ImGuiManager.h"
#include "JsonReader.h"
#include "Localizer.h"
#include <detours.h>
#include <fstream>
#include <nlohmann/json.hpp>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace IIF::UI
{
    namespace {
        constexpr const char* kEditorSettingsPath = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\IIF_EditorSettings.json";
        constexpr std::uint32_t kModifierShift = 1 << 0;
        constexpr std::uint32_t kModifierCtrl = 1 << 1;
        constexpr std::uint32_t kModifierAlt = 1 << 2;
        constexpr UINT kDefaultToggleKey = VK_F11;
        constexpr std::uint32_t kDefaultToggleModifiers = kModifierShift;

        UINT g_toggleKey = kDefaultToggleKey;
        std::uint32_t g_toggleModifiers = kDefaultToggleModifiers;
        bool g_captureToggleKey = false;

        bool IsModifierKey(UINT key) {
            return key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
                key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
                key == VK_MENU || key == VK_LMENU || key == VK_RMENU;
        }

        std::uint32_t CurrentWin32ModifierMask() {
            std::uint32_t mods = 0;
            if (GetKeyState(VK_SHIFT) & 0x8000) mods |= kModifierShift;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= kModifierCtrl;
            if (GetKeyState(VK_MENU) & 0x8000) mods |= kModifierAlt;
            return mods;
        }

        const char* KeyName(UINT key) {
            switch (key) {
            case VK_F1: return "F1";
            case VK_F2: return "F2";
            case VK_F3: return "F3";
            case VK_F4: return "F4";
            case VK_F5: return "F5";
            case VK_F6: return "F6";
            case VK_F7: return "F7";
            case VK_F8: return "F8";
            case VK_F9: return "F9";
            case VK_F10: return "F10";
            case VK_F11: return "F11";
            case VK_F12: return "F12";
            case VK_INSERT: return "Insert";
            case VK_DELETE: return "Delete";
            case VK_HOME: return "Home";
            case VK_END: return "End";
            case VK_PRIOR: return "PageUp";
            case VK_NEXT: return "PageDown";
            case VK_OEM_3: return "`";
            case VK_OEM_MINUS: return "-";
            case VK_OEM_PLUS: return "=";
            case VK_OEM_4: return "[";
            case VK_OEM_6: return "]";
            case VK_OEM_5: return "\\";
            case VK_OEM_1: return ";";
            case VK_OEM_7: return "'";
            case VK_OEM_COMMA: return ",";
            case VK_OEM_PERIOD: return ".";
            case VK_OEM_2: return "/";
            default:
                static char label[16];
                if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) {
                    label[0] = static_cast<char>(key);
                    label[1] = '\0';
                    return label;
                }
                snprintf(label, sizeof(label), "VK_%02X", static_cast<unsigned>(key));
                return label;
            }
        }

        std::string ShortcutLabel(UINT key = g_toggleKey, std::uint32_t modifiers = g_toggleModifiers) {
            std::string text;
            if (modifiers & kModifierCtrl) text += "Ctrl+";
            if (modifiers & kModifierShift) text += "Shift+";
            if (modifiers & kModifierAlt) text += "Alt+";
            text += KeyName(key);
            return text;
        }

        bool IsToggleShortcut(UINT key) {
            return key == g_toggleKey && CurrentWin32ModifierMask() == g_toggleModifiers;
        }

        void SaveEditorSettings() {
            try {
                std::filesystem::create_directories(std::filesystem::path(kEditorSettingsPath).parent_path());
                nlohmann::json root;
                root["menuToggleKey"] = g_toggleKey;
                root["menuToggleModifiers"] = g_toggleModifiers;
                std::ofstream file(kEditorSettingsPath);
                file << root.dump(2);
            }
            catch (const std::exception& e) {
                REX::WARN("[IIF] Failed to save editor settings: {}", e.what());
            }
            catch (...) {
                REX::WARN("[IIF] Failed to save editor settings.");
            }
        }

        void LoadEditorSettings() {
            g_toggleKey = kDefaultToggleKey;
            g_toggleModifiers = kDefaultToggleModifiers;
            try {
                std::ifstream file(kEditorSettingsPath);
                if (!file.is_open()) {
                    SaveEditorSettings();
                    return;
                }
                auto root = nlohmann::json::parse(file, nullptr, true, true);
                if (!root.is_object()) return;
                auto key = root.value("menuToggleKey", static_cast<std::uint32_t>(kDefaultToggleKey));
                auto modifiers = root.value("menuToggleModifiers", kDefaultToggleModifiers);
                if (key > 0 && key <= 0xFF && !IsModifierKey(static_cast<UINT>(key))) {
                    g_toggleKey = static_cast<UINT>(key);
                    g_toggleModifiers = modifiers & (kModifierShift | kModifierCtrl | kModifierAlt);
                }
            }
            catch (const std::exception& e) {
                REX::WARN("[IIF] Failed to load editor settings: {}", e.what());
            }
            catch (...) {
                REX::WARN("[IIF] Failed to load editor settings.");
            }
        }
    }

    static bool InputTextString(const char* label, std::string& str) {
        char buf[1024];
        strcpy_s(buf, sizeof(buf), str.c_str());
        if (ImGui::InputText(label, buf, sizeof(buf))) {
            str = buf;
            return true;
        }
        return false;
    }

    static bool ColorEdit3U32(const char* label, std::uint32_t* color) {
        float col[3] = { ((*color >> 16) & 0xFF) / 255.0f, ((*color >> 8) & 0xFF) / 255.0f, ((*color) & 0xFF) / 255.0f };
        if (ImGui::ColorEdit3(label, col, ImGuiColorEditFlags_NoInputs)) {
            *color = (static_cast<std::uint32_t>(col[0] * 255.0f) << 16) | (static_cast<std::uint32_t>(col[1] * 255.0f) << 8) | (static_cast<std::uint32_t>(col[2] * 255.0f));
            return true;
        }
        return false;
    }

    struct UIRuleProxy {
        int type;
        std::string id;
        std::string label;
        int* priority;
        std::vector<std::string>* anchorTargets;
        std::vector<std::string>* anchorModes;
        bool* isOverridden;
    };

    struct VanillaDef { std::string id; std::string label; };
    static std::vector<VanillaDef> masterVanillaList = {
        {"$dmg", "原版：伤害"}, {"$Melee", "原版：近战伤害"}, {"$Armor", "原版：护甲/抗性"},
        {"$ammo", "原版：弹药"}, {"$fireRate", "原版：射速"}, {"$rng", "原版：射程"},
        {"$acc", "原版：命中率"}, {"$speed", "原版：攻击速度"}, {"$wt", "原版：重量"}, {"$val", "原版：价值"}
    };

    static bool IsRootNode(const std::string& id) {
        if (id == "TOP" || id == "BOTTOM") return true;
        for (const auto& v : masterVanillaList) { if (v.id == id) return true; }
        return false;
    }

    auto DrawAnchorChain = [](std::vector<std::string>& targets, std::vector<std::string>& modes, bool& changed, const std::string& currentCardId, const std::vector<UIRuleProxy>& allMods) {
        ImGui::TextColored({ 1, 1, 0, 1 }, "锚点目标链 (优先级从上到下):");
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("AnchorList", ImVec2(0, 140), true);

        std::vector<std::string> validTargets = { "TOP", "BOTTOM" };
        for (auto& v : masterVanillaList) validTargets.push_back(v.id);
        for (auto& m : allMods) { if (m.id != currentCardId) validTargets.push_back(m.id); }

        for (int a = 0; a < (int)targets.size(); ++a) {
            ImGui::PushID(a);
            ImGui::SetNextItemWidth(140);
            if (ImGui::BeginCombo("##Tgt", targets[a].c_str())) {
                for (const auto& tgtOpt : validTargets) {
                    if (ImGui::Selectable(tgtOpt.c_str(), targets[a] == tgtOpt)) {
                        targets[a] = tgtOpt; changed = true;
                        if (!IsRootNode(targets[a])) modes[a] = "after";
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();

            if (IsRootNode(targets[a])) {
                ImGui::SetNextItemWidth(100);
                const char* mdOpts[] = { "after(后置)", "before(前置)", "replace(替换)" };
                int curMode = (modes[a] == "before") ? 1 : (modes[a] == "replace" ? 2 : 0);
                if (ImGui::Combo("##Mode", &curMode, mdOpts, 3)) {
                    modes[a] = (curMode == 1) ? "before" : (curMode == 2 ? "replace" : "after");
                    changed = true;
                }
            }
            else {
                modes[a] = "after";
                ImGui::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, " [锁定为子层]");
            }

            ImGui::SameLine();
            if (ImGui::Button("X")) {
                targets.erase(targets.begin() + a);
                modes.erase(modes.begin() + a);
                changed = true;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("+ 添加备选锚点 (Add Target)")) { targets.push_back("BOTTOM"); modes.push_back("after"); changed = true; }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        };

    ImGuiManager::WndProc_t ImGuiManager::m_originalWndProc = nullptr;
    void* ImGuiManager::m_originalPresentVtEntry = nullptr;

    HWND ImGuiManager::m_windowHandle = nullptr;
    ID3D11Device* ImGuiManager::m_pDevice = nullptr;
    ID3D11DeviceContext* ImGuiManager::m_pContext = nullptr;

    bool ImGuiManager::m_isInitialized = false;
    bool ImGuiManager::m_isVisible = false;

    void ImGuiManager::ToggleDisplay() {
        m_isVisible = !m_isVisible;
        if (m_isInitialized) ImGui::GetIO().MouseDrawCursor = m_isVisible;
        auto controlMap = RE::ControlMap::GetSingleton();
        auto menuCursor = RE::MenuCursor::GetSingleton();
        auto ui = RE::UI::GetSingleton();
        if (m_isVisible) {
            if (controlMap) controlMap->PushInputContext(RE::UserEvents::INPUT_CONTEXT_ID::kCursor);
            if (menuCursor) menuCursor->RegisterCursor();
            if (ui) ui->menuMode++;
        }
        else {
            if (controlMap) controlMap->PopInputContext(RE::UserEvents::INPUT_CONTEXT_ID::kCursor);
            if (menuCursor) menuCursor->UnregisterCursor();
            if (ui && ui->menuMode > 0) ui->menuMode--;
        }
    }

    bool ImGuiManager::Install() {
        if (m_originalPresentVtEntry) return true;
        LoadEditorSettings();
        REX::INFO("[IIF] ImGui editor toggle hotkey: {}", ShortcutLabel());
        auto rd = RE::BSGraphics::GetRendererData();
        if (!rd || !rd->renderWindow[0].swapChain) return false;

        // 从渲染器数据获取游戏真正的 D3D11 设备（不走 pSwapChain->GetDevice，
        // DLSS FG 下后者返回 NVIDIA 包装设备，ImGui 用它在 Present 里渲染会死锁）
        if (rd->device) m_pDevice = reinterpret_cast<ID3D11Device*>(rd->device);
        if (rd->context) m_pContext = reinterpret_cast<ID3D11DeviceContext*>(rd->context);

        auto sc = reinterpret_cast<IDXGISwapChain*>(rd->renderWindow[0].swapChain);
        void** vt = *reinterpret_cast<void***>(sc);
        m_originalPresentVtEntry = vt[8];

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&m_originalPresentVtEntry, reinterpret_cast<void*>(Present_Hook));
        DetourTransactionCommit();
        return true;
    }

    void ImGuiManager::RenderCore(IDXGISwapChain* pSwapChain) {
        if (!m_originalPresentVtEntry) return;
        DXGI_SWAP_CHAIN_DESC sd;
        if (FAILED(pSwapChain->GetDesc(&sd))) return;
        HWND currentHwnd = sd.OutputWindow;

        if (currentHwnd && currentHwnd != m_windowHandle) {
            m_windowHandle = currentHwnd;
            m_originalWndProc = (WndProc_t)SetWindowLongPtr(m_windowHandle, GWLP_WNDPROC, (LONG_PTR)WndProc_Hook);
            if (m_isInitialized) { ImGui_ImplWin32_Shutdown(); ImGui_ImplWin32_Init(m_windowHandle); }
        }

        if (!m_isInitialized && m_windowHandle) {
            if (m_pDevice && m_pContext) {
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                static std::string iniPath = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\IIF_Layout.ini";
                io.IniFilename = iniPath.c_str();
                static const ImWchar glyphRanges[] = {
                    0x0020, 0x00FF,
                    0x2000, 0x206F,
                    0x3000, 0x30FF,
                    0x31F0, 0x31FF,
                    0xFF00, 0xFFEF,
                    0xFFFD, 0xFFFD,
                    0x4e00, 0x9FFF,
                    0,
                };
                char fontPath[MAX_PATH];
                GetWindowsDirectoryA(fontPath, MAX_PATH);
                strcat_s(fontPath, "\\Fonts\\msyh.ttc");
                if (std::filesystem::exists(fontPath)) {
                    if (auto* font = io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, glyphRanges))
                        io.FontDefault = font;
                } else {
                    char fontPathSimSun[MAX_PATH];
                    GetWindowsDirectoryA(fontPathSimSun, MAX_PATH);
                    strcat_s(fontPathSimSun, "\\Fonts\\simsun.ttc");
                    if (std::filesystem::exists(fontPathSimSun)) {
                        if (auto* font = io.Fonts->AddFontFromFileTTF(fontPathSimSun, 18.0f, nullptr, glyphRanges))
                            io.FontDefault = font;
                    } else {
                        std::string customFont = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\fonts\\iui_font.ttf";
                        if (std::filesystem::exists(customFont)) {
                            if (auto* font = io.Fonts->AddFontFromFileTTF(customFont.c_str(), 18.0f, nullptr, glyphRanges))
                                io.FontDefault = font;
                        } else {
                            io.Fonts->AddFontDefault();
                        }
                    }
                }
                ImGui_ImplWin32_Init(m_windowHandle); ImGui_ImplDX11_Init(m_pDevice, m_pContext);
                ImGui::StyleColorsDark(); m_isInitialized = true;
            }
        }
        if (!m_isInitialized) return;

        if (m_isVisible) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

        static std::string selectedID = "";
        static int selectedType = -1;
        static std::string pendingDeleteID = "";

        static bool pendingAdd = false;
        static std::string pendingAddID = "";
        static std::string pendingAddFile = "";
        static int pendingAddType = 0;

        static char searchBuffer[256] = "";
        static bool filterOnlyCPP = false;
        static bool filterOnlyJSON = false;
        static bool filterOnlyOverridden = false;

        if (m_isVisible) {
            if (pendingAdd) {
                IIF::JsonReader::JsonRule newRule;
                newRule.id = pendingAddID;
                newRule.titleText = "新建信息卡";
                newRule.priority = 800;
                newRule.anchorTargets.push_back("BOTTOM");
                newRule.anchorModes.push_back("after");
                newRule.displayType = pendingAddType;
                newRule.originPath = pendingAddFile;

                IIF::JsonReader::g_rules.push_back(newRule);
                selectedID = pendingAddID;
                selectedType = 0;

                IIF::JsonReader::SaveConfigs();
                pendingAdd = false; pendingAddID = ""; pendingAddFile = ""; pendingAddType = 0;
            }

            if (!pendingDeleteID.empty()) {
                auto it = std::find_if(IIF::JsonReader::g_rules.begin(), IIF::JsonReader::g_rules.end(), [&](const auto& r) { return r.id == pendingDeleteID; });
                if (it != IIF::JsonReader::g_rules.end()) {
                    std::string pathToDelete = it->originPath;
                    IIF::JsonReader::g_rules.erase(it);

                    bool fileStillInUse = false;
                    for (const auto& r : IIF::JsonReader::g_rules) {
                        if (r.originPath == pathToDelete) { fileStillInUse = true; break; }
                    }
                    if (!fileStillInUse && !pathToDelete.empty()) {
                        try {
                            std::string actualPath = pathToDelete;
                            if (actualPath.find("Data\\F4SE") == std::string::npos && actualPath.find("Data/F4SE") == std::string::npos) {
                                actualPath = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\" + actualPath;
                            }
                            if (std::filesystem::exists(actualPath)) std::filesystem::remove(actualPath);
                        }
                        catch (...) {}
                    }
                }
                if (selectedID == pendingDeleteID) { selectedID = ""; selectedType = -1; }
                IIF::JsonReader::SaveConfigs();
                pendingDeleteID = "";
            }

            auto& rules = IIF::JsonReader::g_rules;
            auto& cppOverrides = IIF::JsonReader::g_cppOverrides;

            std::vector<UIRuleProxy> modProxies;
            for (int i = 0; i < rules.size(); ++i) modProxies.push_back({ 0, rules[i].id, "", &rules[i].priority, &rules[i].anchorTargets, &rules[i].anchorModes, nullptr });
            for (auto& [id, over] : cppOverrides) {
                if (!over.isRegistered) continue;
                modProxies.push_back({ 1, id, "", &over.userPriority, &over.userAnchorTargets, &over.userAnchorModes, &over.isOverridden });
            }

            static int currentScenario = 0;
            std::vector<VanillaDef> scenarioGuns = { {"$dmg", "原版：伤害"}, {"$ammo", "原版：弹药"}, {"$fireRate", "原版：射速"}, {"$rng", "原版：射程"}, {"$acc", "原版：命中率"}, {"$wt", "原版：重量"}, {"$val", "原版：价值"} };
            std::vector<VanillaDef> scenarioMelee = { {"$Melee", "原版：近战伤害"}, {"$speed", "原版：攻击速度"}, {"$wt", "原版：重量"}, {"$val", "原版：价值"} };
            std::vector<VanillaDef> scenarioArmor = { {"$Armor", "原版：伤害抗性"}, {"$wt", "原版：重量"}, {"$val", "原版：价值"} };
            std::vector<VanillaDef> scenarioAid = { {"$wt", "原版：重量"}, {"$val", "原版：价值"} };

            std::vector<VanillaDef>* activeScenarioList = &scenarioGuns;
            if (currentScenario == 1) activeScenarioList = &scenarioMelee;
            else if (currentScenario == 2) activeScenarioList = &scenarioArmor;
            else if (currentScenario == 3) activeScenarioList = &scenarioAid;

            struct RootGroup { std::vector<UIRuleProxy*> before; std::vector<UIRuleProxy*> replace; std::vector<UIRuleProxy*> after; };
            std::map<std::string, RootGroup> rootMap;
            std::map<std::string, std::vector<UIRuleProxy*>> childrenMap;

            for (auto& p : modProxies) {
                std::string activeAnchor = "BOTTOM"; std::string activeMode = "after";
                for (size_t tIdx = 0; tIdx < p.anchorTargets->size(); ++tIdx) {
                    std::string tgt = (*p.anchorTargets)[tIdx];
                    std::string mode = "after";
                    if (!p.anchorModes->empty()) mode = (tIdx < p.anchorModes->size()) ? (*p.anchorModes)[tIdx] : p.anchorModes->back();

                    if (tgt == "TOP" || tgt == "BOTTOM") { activeAnchor = tgt; activeMode = mode; break; }
                    bool found = false;
                    for (auto& vc : *activeScenarioList) { if (vc.id == tgt) { found = true; break; } }
                    if (!found) { for (auto& mp : modProxies) { if (mp.id == tgt) { found = true; break; } } }
                    if (found) { activeAnchor = tgt; activeMode = mode; break; }
                }

                if (IsRootNode(activeAnchor)) {
                    if (activeMode == "before") rootMap[activeAnchor].before.push_back(&p);
                    else if (activeMode == "replace") rootMap[activeAnchor].replace.push_back(&p);
                    else rootMap[activeAnchor].after.push_back(&p);
                }
                else {
                    childrenMap[activeAnchor].push_back(&p);
                }
            }

            auto sortPriority = [](UIRuleProxy* a, UIRuleProxy* b) { return *(a->priority) < *(b->priority); };
            for (auto& pair : rootMap) {
                std::sort(pair.second.before.begin(), pair.second.before.end(), sortPriority);
                std::sort(pair.second.replace.begin(), pair.second.replace.end(), sortPriority);
                std::sort(pair.second.after.begin(), pair.second.after.end(), sortPriority);
            }
            for (auto& pair : childrenMap) {
                std::sort(pair.second.begin(), pair.second.end(), sortPriority);
            }

            std::unordered_set<std::string> reachableNodes;
            std::function<void(const std::string&)> MarkReachable = [&](const std::string& id) {
                if (reachableNodes.count(id)) return;
                reachableNodes.insert(id);
                if (rootMap.count(id)) {
                    for (auto* p : rootMap[id].before) MarkReachable(p->id);
                    for (auto* p : rootMap[id].replace) MarkReachable(p->id);
                    for (auto* p : rootMap[id].after) MarkReachable(p->id);
                }
                if (childrenMap.count(id)) {
                    for (auto* p : childrenMap[id]) MarkReachable(p->id);
                }
                };
            for (auto& vc : *activeScenarioList) MarkReachable(vc.id);
            MarkReachable("TOP"); MarkReachable("BOTTOM");

            // ==============================================================
            // 👑 绘制主编辑器面板
            // ==============================================================
            ImGui::SetNextWindowSize(ImVec2(1000, 750), ImGuiCond_FirstUseEver);
            if (ImGui::Begin(_L("UI_WindowTitle"), nullptr)) {

                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, _L("UI_StatusNormal"));
                ImGui::SameLine(ImGui::GetWindowWidth() - 160.0f);
                ImGui::SetNextItemWidth(140.0f);
                std::string currentLang = IIF::L10n::GetCurrentLanguage();
                if (ImGui::BeginCombo("##LangCombo", currentLang.c_str())) {
                    for (const auto& lang : IIF::L10n::GetAvailableLanguages()) {
                        if (ImGui::Selectable(lang.c_str(), currentLang == lang)) IIF::L10n::LoadLanguage(lang);
                    }
                    ImGui::EndCombo();
                }
                ImGui::Separator();
                if (ImGui::Button(_L("UI_BtnReload"), { 180, 32 })) { IIF::JsonReader::LoadConfigs(); IIF::L10n::LoadLanguage(IIF::L10n::GetCurrentLanguage()); }
                ImGui::SameLine();
                if (ImGui::Button("保存所有修改 (Save)", { 180, 32 })) { IIF::JsonReader::SaveConfigs(); }
                ImGui::SameLine();
                ImGui::Text("Editor hotkey: %s", ShortcutLabel().c_str());
                if (ImGui::Button(g_captureToggleKey ? "Press a key... (Esc cancel)" : "Change editor hotkey", { 220, 28 })) {
                    g_captureToggleKey = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Shift+F11", { 150, 28 })) {
                    g_toggleKey = kDefaultToggleKey;
                    g_toggleModifiers = kDefaultToggleModifiers;
                    g_captureToggleKey = false;
                    SaveEditorSettings();
                    REX::INFO("[IIF] ImGui editor toggle hotkey reset to {}", ShortcutLabel());
                }
                bool useCtrl = (g_toggleModifiers & kModifierCtrl) != 0;
                bool useShift = (g_toggleModifiers & kModifierShift) != 0;
                bool useAlt = (g_toggleModifiers & kModifierAlt) != 0;
                bool modifiersChanged = false;
                modifiersChanged |= ImGui::Checkbox("Ctrl##EditorHotkey", &useCtrl);
                ImGui::SameLine();
                modifiersChanged |= ImGui::Checkbox("Shift##EditorHotkey", &useShift);
                ImGui::SameLine();
                modifiersChanged |= ImGui::Checkbox("Alt##EditorHotkey", &useAlt);
                if (modifiersChanged) {
                    g_toggleModifiers = 0;
                    if (useCtrl) g_toggleModifiers |= kModifierCtrl;
                    if (useShift) g_toggleModifiers |= kModifierShift;
                    if (useAlt) g_toggleModifiers |= kModifierAlt;
                    SaveEditorSettings();
                    REX::INFO("[IIF] ImGui editor toggle hotkey changed to {}", ShortcutLabel());
                }
                ImGui::Spacing();

                ImGui::BeginChild("RuleList", ImVec2(480, 0), true);

                ImGui::PushItemWidth(460);
                ImGui::Combo("##Scenario", &currentScenario, "模拟场景：枪械武器\0模拟场景：近战武器\0模拟场景：护甲服装\0模拟场景：消耗品\0\0");
                ImGui::PopItemWidth();

                ImGui::Spacing();
                ImGui::PushItemWidth(460);
                ImGui::InputTextWithHint("##Search", "检索信息卡 ID 或名称...", searchBuffer, 256);
                ImGui::PopItemWidth();

                ImGui::Checkbox("仅 C++", &filterOnlyCPP); ImGui::SameLine();
                ImGui::Checkbox("仅 JSON", &filterOnlyJSON); ImGui::SameLine();
                ImGui::Checkbox("仅受修改项", &filterOnlyOverridden);
                ImGui::Separator();

                if (filterOnlyCPP) filterOnlyJSON = false;
                if (filterOnlyJSON) filterOnlyCPP = false;

                ImGui::Spacing();
                if (ImGui::Button("+ 新建自定义信息卡 (Create New Card)", ImVec2(-1, 30))) { ImGui::OpenPopup("CreateNewCardPopup"); }

                static char newCardFileBuf[256] = "";
                static int newCardType = 0;
                if (ImGui::BeginPopupModal("CreateNewCardPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("1. 请输入新建信息卡的名称 (推荐全英文):");
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(340);
                    ImGui::InputText("##NewCardFile", newCardFileBuf, 256);

                    std::string rawInput = newCardFileBuf;
                    std::string baseId = rawInput;
                    size_t extPos = baseId.find(".json");
                    if (extPos != std::string::npos) baseId = baseId.substr(0, extPos);

                    bool idExists = false;
                    if (baseId == "TOP" || baseId == "BOTTOM") idExists = true;
                    for (auto& v : masterVanillaList) if (v.id == baseId) idExists = true;
                    for (auto& r : IIF::JsonReader::g_rules) if (r.id == baseId) idExists = true;
                    for (auto& p : IIF::JsonReader::g_cppOverrides) if (p.first == baseId) idExists = true;

                    if (idExists) ImGui::TextColored({ 1, 0.3f, 0.3f, 1 }, "该名称已存在！请更换名称以免冲突。");
                    else if (baseId.length() > 0) ImGui::TextColored({ 0.5f, 1.0f, 0.5f, 1 }, "名称可用，创建后将立即生效。");

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    ImGui::Text("2. 请选择信息卡的排版类型:");
                    ImGui::Spacing(); ImGui::SetNextItemWidth(340);
                    const char* typeOpts[] = { "普通文本/数值排版 (Type 0)", "进度条可视化排版 (Type 1)", "左右双图标高阶排版 (Type 2)" };
                    ImGui::Combo("##NewCardType", &newCardType, typeOpts, 3);
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    bool canCreate = !idExists && baseId.length() > 0;
                    if (ImGui::Button("立即创建", ImVec2(160, 30)) && canCreate) {
                        pendingAddID = baseId;
                        pendingAddFile = baseId + ".json";
                        pendingAddType = newCardType;
                        pendingAdd = true;
                        newCardFileBuf[0] = '\0'; newCardType = 0;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("取消", ImVec2(160, 30))) {
                        newCardFileBuf[0] = '\0'; newCardType = 0;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                std::string searchStr = searchBuffer;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

                auto MatchesFilter = [&](UIRuleProxy* p) {
                    if (filterOnlyCPP && p->type != 1) return false;
                    if (filterOnlyJSON && p->type != 0) return false;
                    if (filterOnlyOverridden && p->type == 1 && !*(p->isOverridden)) return false;
                    if (!searchStr.empty()) {
                        std::string lowerId = p->id;
                        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
                        if (lowerId.find(searchStr) == std::string::npos) return false;
                    }
                    return true;
                    };

                static std::unordered_set<std::string> foldedNodes;
                std::unordered_set<std::string> nodesToDisplay;
                bool isFiltering = !searchStr.empty() || filterOnlyCPP || filterOnlyJSON || filterOnlyOverridden;

                std::function<bool(UIRuleProxy*)> CheckAndMarkVisible = [&](UIRuleProxy* p) {
                    bool selfMatch = !isFiltering || MatchesFilter(p);
                    bool childMatch = false;
                    if (childrenMap.count(p->id)) {
                        for (auto* child : childrenMap[p->id]) {
                            if (CheckAndMarkVisible(child)) childMatch = true;
                        }
                    }
                    if (selfMatch || childMatch) {
                        nodesToDisplay.insert(p->id);
                        if (childMatch && isFiltering) foldedNodes.erase(p->id);
                        return true;
                    }
                    return false;
                    };

                for (auto& pair : rootMap) {
                    for (auto* p : pair.second.before) CheckAndMarkVisible(p);
                    for (auto* p : pair.second.replace) CheckAndMarkVisible(p);
                    for (auto* p : pair.second.after) CheckAndMarkVisible(p);
                }
                for (auto& pair : childrenMap) {
                    for (auto* p : pair.second) {
                        if (!reachableNodes.count(p->id)) CheckAndMarkVisible(p);
                    }
                }

                auto ToggleFold = [&](const std::string& id, bool state) { if (state) foldedNodes.insert(id); else foldedNodes.erase(id); };
                std::function<void(const std::string&, bool)> SetFoldRecursive = [&](const std::string& id, bool foldState) {
                    ToggleFold(id, foldState);
                    if (rootMap.count(id)) {
                        for (auto* p : rootMap[id].before) SetFoldRecursive(p->id, foldState);
                        for (auto* p : rootMap[id].replace) SetFoldRecursive(p->id, foldState);
                        for (auto* p : rootMap[id].after) SetFoldRecursive(p->id, foldState);
                    }
                    if (childrenMap.count(id)) { for (auto* p : childrenMap[id]) SetFoldRecursive(p->id, foldState); }
                    };

                std::unordered_set<std::string> visitedModCards;
                ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

                std::function<void(UIRuleProxy*, const std::string&, const std::string&, const std::string&, const char*, const std::string&, const std::string&, bool)> DrawModTree =
                    [&](UIRuleProxy* p, const std::string& currentSpine, const std::string& currentBranch, const std::string& spineForChildren, const char* modeStr, const std::string& parentAnchorId, const std::string& parentAnchorMode, bool isParentHighlighted)
                    {
                        if (visitedModCards.count(p->id)) return; visitedModCards.insert(p->id);
                        if (!nodesToDisplay.count(p->id)) return;

                        std::vector<UIRuleProxy*> visibleChildren;
                        if (childrenMap.count(p->id)) {
                            for (auto* child : childrenMap[p->id]) {
                                if (nodesToDisplay.count(child->id)) visibleChildren.push_back(child);
                            }
                        }

                        bool hasChildren = !visibleChildren.empty();
                        bool isFolded = foldedNodes.count(p->id);
                        bool isCurrentlyOpen = !isFolded;
                        bool isCurrentSelected = (selectedID == p->id && selectedType == p->type);
                        bool passHighlight = isParentHighlighted || isCurrentSelected;

                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (isCurrentSelected) flags |= ImGuiTreeNodeFlags_Selected;
                        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        else ImGui::SetNextItemOpen(isCurrentlyOpen, ImGuiCond_Always);

                        std::string nodeHiddenId = "##MOD_" + p->id;
                        bool isOpen = ImGui::TreeNodeEx(nodeHiddenId.c_str(), flags, "");

                        if (hasChildren && isOpen != isCurrentlyOpen) {
                            if (isOpen) foldedNodes.erase(p->id); else foldedNodes.insert(p->id);
                            isCurrentlyOpen = isOpen;
                        }

                        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { selectedID = p->id; selectedType = p->type; }

                        if (hasChildren && ImGui::IsItemHovered()) {
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl) SetFoldRecursive(p->id, !isCurrentlyOpen);
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) SetFoldRecursive(p->id, !isCurrentlyOpen);
                        }

                        if (ImGui::BeginDragDropSource()) {
                            ImGui::SetDragDropPayload("MOD_CARD", &p, sizeof(UIRuleProxy*));
                            ImGui::Text("正在移动: %s", p->id.c_str());
                            ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, ImGui::GetIO().KeyShift ? "[!] 松开以作为子层挂载" : "[=] 松开以排序");
                            ImGui::EndDragDropSource();
                        }

                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOD_CARD")) {
                                UIRuleProxy* src = *(UIRuleProxy**)payload->Data;
                                if (src->id != p->id) {
                                    if (ImGui::GetIO().KeyShift) {
                                        src->anchorTargets->clear(); src->anchorTargets->push_back(p->id);
                                        src->anchorModes->clear(); src->anchorModes->push_back("after");
                                    }
                                    else {
                                        src->anchorTargets->clear(); src->anchorTargets->push_back(parentAnchorId);
                                        src->anchorModes->clear(); src->anchorModes->push_back(parentAnchorMode);
                                        int newPriority = *(p->priority) + 1;
                                        *(src->priority) = newPriority;
                                        for (auto& other : modProxies) {
                                            if (other.id != src->id && other.id != p->id) {
                                                if (!other.anchorTargets->empty() && other.anchorTargets->front() == parentAnchorId) {
                                                    if (*(other.priority) >= newPriority) {
                                                        *(other.priority) += 1;
                                                        if (other.type == 1 && other.isOverridden) *(other.isOverridden) = true;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (src->type == 1 && src->isOverridden) *(src->isOverridden) = true;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::SameLine(0, 0);
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "%s", (currentSpine + currentBranch).c_str());
                        ImGui::SameLine(0, 0);

                        std::string modeTag; ImVec4 tagColor;
                        if (std::string(modeStr) == "before") { modeTag = "[前置] "; tagColor = { 0.5f, 0.8f, 1.0f, 1.0f }; }
                        else if (std::string(modeStr) == "replace") { modeTag = "[替换] "; tagColor = { 1.0f, 0.3f, 0.3f, 1.0f }; }
                        else if (std::string(modeStr) == "child") { modeTag = "[子层] "; tagColor = { 0.8f, 0.5f, 1.0f, 1.0f }; }
                        else { modeTag = "[后置] "; tagColor = { 1.0f, 0.8f, 0.5f, 1.0f }; }
                        ImGui::TextColored(tagColor, "%s", modeTag.c_str()); ImGui::SameLine(0, 0);

                        std::string prefix = (p->type == 1) ? "[C++] " : "[JSON] ";
                        std::string label = prefix + p->id;

                        bool isSelfMatched = MatchesFilter(p) && isFiltering;
                        if (isSelfMatched) ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", label.c_str());
                        else if (isParentHighlighted && !isCurrentSelected) ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", label.c_str());
                        else ImGui::Text("%s", label.c_str());

                        if (isOpen && hasChildren) {
                            for (size_t i = 0; i < visibleChildren.size(); ++i) {
                                bool childIsLast = (i == visibleChildren.size() - 1);
                                std::string childBranch = childIsLast ? "┗━ " : "┣━ ";
                                std::string nextSpineForChildren = spineForChildren + (childIsLast ? "    " : "┃   ");
                                DrawModTree(visibleChildren[i], spineForChildren, childBranch, nextSpineForChildren, "child", p->id, "after", passHighlight);
                            }
                            ImGui::TreePop();
                        }
                    };

                auto DrawRootZoneContent = [&](const std::string& anchorId, bool isVanillaAnchor, const std::string& vLabel) {
                    auto& z = rootMap[anchorId];
                    std::vector<UIRuleProxy*> visibleBefore;
                    std::vector<UIRuleProxy*> visibleReplace;
                    std::vector<UIRuleProxy*> visibleAfter;

                    for (auto* p : z.before) if (nodesToDisplay.count(p->id)) visibleBefore.push_back(p);
                    for (auto* p : z.replace) if (nodesToDisplay.count(p->id)) visibleReplace.push_back(p);
                    for (auto* p : z.after) if (nodesToDisplay.count(p->id)) visibleAfter.push_back(p);

                    bool hasVisibleChildren = !visibleBefore.empty() || !visibleReplace.empty() || !visibleAfter.empty();

                    if (isFiltering && !hasVisibleChildren) return;
                    if (!isVanillaAnchor && !hasVisibleChildren) return;

                    if (hasVisibleChildren) ImGui::Dummy(ImVec2(0, 4.0f));

                    bool isFolded = foldedNodes.count(anchorId);
                    if (isFiltering && hasVisibleChildren) { foldedNodes.erase(anchorId); isFolded = false; }

                    bool isCurrentlyOpen = !isFolded;
                    bool isCurrentSelected = (selectedID == anchorId && selectedType == 2);

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (isCurrentSelected) flags |= ImGuiTreeNodeFlags_Selected;

                    if (!hasVisibleChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    else ImGui::SetNextItemOpen(isCurrentlyOpen, ImGuiCond_Always);

                    if (!isVanillaAnchor) {
                        ImVec4 col = (anchorId == "TOP") ? ImVec4(1, 0.5f, 0, 1) : ImVec4(1, 0, 0, 1);
                        ImGui::PushStyleColor(ImGuiCol_Text, col);
                        std::string zoneHiddenId = "##ZONE_" + anchorId;
                        bool isOpen = ImGui::TreeNodeEx(zoneHiddenId.c_str(), flags);
                        ImGui::PopStyleColor();

                        if (hasVisibleChildren && isOpen != isCurrentlyOpen) {
                            if (isOpen) foldedNodes.erase(anchorId); else foldedNodes.insert(anchorId);
                            isCurrentlyOpen = isOpen;
                        }

                        if (hasVisibleChildren && ImGui::IsItemHovered()) {
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl) SetFoldRecursive(anchorId, !isCurrentlyOpen);
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) SetFoldRecursive(anchorId, !isCurrentlyOpen);
                        }

                        ImGui::SameLine(0, 0);
                        ImGui::TextColored(col, "%s", anchorId == "TOP" ? "[顶部强制置顶区]" : "[底部强制兜底区]");

                        if (isOpen && hasVisibleChildren) {
                            size_t totalChildren = visibleBefore.size() + visibleReplace.size() + visibleAfter.size();
                            size_t count = 0;
                            auto DrawGroup = [&](std::vector<UIRuleProxy*>& group, const char* mode) {
                                for (auto* p : group) {
                                    bool isLast = (count == totalChildren - 1);
                                    std::string branch = isLast ? "┗━ " : "┣━ ";
                                    std::string spineForChildren = isLast ? "    " : "┃   ";
                                    DrawModTree(p, "", branch, spineForChildren, mode, anchorId, mode, false);
                                    count++;
                                }
                                };
                            DrawGroup(visibleBefore, "before");
                            DrawGroup(visibleReplace, "replace");
                            DrawGroup(visibleAfter, "after");
                            ImGui::TreePop();
                        }
                    }
                    else {
                        std::string vanillaHiddenId = "##VANILLA_" + anchorId;
                        bool isOpen = ImGui::TreeNodeEx(vanillaHiddenId.c_str(), flags);

                        if (hasVisibleChildren && isOpen != isCurrentlyOpen) {
                            if (isOpen) foldedNodes.erase(anchorId); else foldedNodes.insert(anchorId);
                            isCurrentlyOpen = isOpen;
                        }

                        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && isVanillaAnchor) {
                            selectedID = anchorId; selectedType = 2;
                        }

                        if (hasVisibleChildren && ImGui::IsItemHovered()) {
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl) SetFoldRecursive(anchorId, !isCurrentlyOpen);
                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) SetFoldRecursive(anchorId, !isCurrentlyOpen);
                        }

                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOD_CARD")) {
                                UIRuleProxy* src = *(UIRuleProxy**)payload->Data;
                                src->anchorTargets->clear(); src->anchorTargets->push_back(anchorId);
                                src->anchorModes->clear(); src->anchorModes->push_back("after");
                                if (src->type == 1 && src->isOverridden) *(src->isOverridden) = true;
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::SameLine(0, 0);

                        std::string label = "[原版大区] " + vLabel;
                        ImGui::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", label.c_str());

                        if (isOpen && hasVisibleChildren) {
                            size_t totalChildren = visibleBefore.size() + visibleReplace.size() + visibleAfter.size();
                            size_t count = 0;
                            auto DrawGroup = [&](std::vector<UIRuleProxy*>& group, const char* mode) {
                                for (auto* p : group) {
                                    bool isLast = (count == totalChildren - 1);
                                    std::string branch = isLast ? "┗━ " : "┣━ ";
                                    std::string spineForChildren = isLast ? "    " : "┃   ";
                                    DrawModTree(p, "", branch, spineForChildren, mode, anchorId, mode, isCurrentSelected);
                                    count++;
                                }
                                };
                            DrawGroup(visibleBefore, "before");
                            DrawGroup(visibleReplace, "replace");
                            DrawGroup(visibleAfter, "after");
                            ImGui::TreePop();
                        }
                    }
                    if (hasVisibleChildren) ImGui::Dummy(ImVec2(0, 4.0f));
                    };

                for (auto& vc : *activeScenarioList) {
                    if (vc.id == "$dmg" || vc.id == "$Melee" || vc.id == "$Armor") DrawRootZoneContent(vc.id, true, vc.label);
                }
                DrawRootZoneContent("TOP", false, "");
                for (auto& vc : *activeScenarioList) {
                    if (vc.id != "$dmg" && vc.id != "$Melee" && vc.id != "$Armor") DrawRootZoneContent(vc.id, true, vc.label);
                }
                DrawRootZoneContent("BOTTOM", false, "");

                bool hasOrphans = false;
                for (auto& pair : childrenMap) {
                    for (auto* p : pair.second) {
                        if (!reachableNodes.count(p->id) && !visitedModCards.count(p->id)) {
                            if (nodesToDisplay.count(p->id)) {
                                if (!hasOrphans) { ImGui::Spacing(); ImGui::TextColored({ 1, 0.5f, 0, 1 }, "[错误闭环/游离锚点区]"); hasOrphans = true; }
                                DrawModTree(p, "", "┗━ ", "    ", "child", "", "", false);
                            }
                        }
                    }
                }

                ImGui::PopStyleVar();
                ImGui::EndChild();

                ImGui::SameLine();

                // 👉 右侧：属性编辑面板
                ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
                ImGui::Text(_L("UI_InspectorHeader"));
                ImGui::Separator(); ImGui::Spacing();

                if (!selectedID.empty()) {
                    if (selectedType == 2) {
                        ImGui::TextColored({ 0.6f, 0.6f, 0.6f, 1.0f }, "[原版游戏自带的信息卡]");
                        ImGui::TextDisabled("你不能修改它的样式或数值，但你可以用它作为你 Mod 卡片的锚点。");
                        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                        ImGui::Text("该卡片的真实底层 ID 为: ");
                        ImGui::SameLine(); ImGui::TextColored({ 0, 1, 0, 1 }, "%s", selectedID.c_str());
                    }
                    else if (selectedType == 1) {
                        auto it = cppOverrides.find(selectedID);
                        if (it != cppOverrides.end()) {
                            auto& over = it->second;
                            ImGui::TextColored({ 0.0f, 1.0f, 1.0f, 1.0f }, "[外部 C++ 插件提供的信息卡]");
                            ImGui::TextDisabled("你只能在此覆盖它的 [局部排版权重] 与 [备选锚点链]。");
                            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                            std::string readOnlyId = over.id;
                            InputTextString("C++ 注册 ID (不可改)", readOnlyId);

                            if (ImGui::InputInt("局部优先级 (Local Weight)", &over.userPriority)) over.isOverridden = true;
                            ImGui::Spacing();

                            bool anchorChanged = false;
                            DrawAnchorChain(over.userAnchorTargets, over.userAnchorModes, anchorChanged, over.id, modProxies);
                            if (anchorChanged) over.isOverridden = true;

                            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                            if (over.isOverridden) {
                                if (ImGui::Button("恢复 C++ 作者推荐排版 (Reset to Default)", { 300, 30 })) {
                                    over.userPriority = over.defaultPriority;
                                    over.userAnchorTargets = over.defaultAnchorTargets;
                                    over.userAnchorModes = over.defaultAnchorModes;
                                    over.isOverridden = false;
                                }
                            }
                            else {
                                ImGui::TextColored({ 0, 1, 0, 1 }, "当前正在使用 C++ 作者的推荐排版");
                            }
                        }
                    }
                    else if (selectedType == 0) {
                        auto it = std::find_if(rules.begin(), rules.end(), [&](const IIF::JsonReader::JsonRule& r) { return r.id == selectedID; });
                        if (it != rules.end()) {
                            auto& rule = *it;
                            bool dummyChanged = false;

                            if (ImGui::CollapsingHeader(_L("UI_BasicSettings"), ImGuiTreeNodeFlags_DefaultOpen)) {
                                InputTextString(_L("UI_RuleID"), rule.id);
                                InputTextString(_L("UI_TitleText"), rule.titleText);
                                ImGui::InputInt("局部优先级 (Local Weight)", &rule.priority);
                                DrawAnchorChain(rule.anchorTargets, rule.anchorModes, dummyChanged, rule.id, modProxies);

                                const char* dTypes[] = { _L("UI_DisplayType_0"), _L("UI_DisplayType_1"), _L("UI_DisplayType_2") };
                                ImGui::Combo(_L("UI_DisplayType"), &rule.displayType, dTypes, 3);
                                ImGui::Checkbox(_L("UI_HighlightLabel"), &rule.highlightLabel);
                                ImGui::Checkbox(_L("UI_HasBackground"), &rule.hasBackground);

                                if (ImGui::InputInt(_L("UI_RoundDigits"), &rule.roundDigits)) {
                                    if (rule.roundDigits < -1) rule.roundDigits = -1;
                                    if (rule.roundDigits > 6) rule.roundDigits = 6;
                                }
                                ImGui::TextDisabled("%s", _L("UI_RoundDigitsHint"));

                                if (ImGui::TreeNodeEx(_L("UI_Menus"), ImGuiTreeNodeFlags_DefaultOpen)) {
                                    const char* menuKeys[] = { "ExamineMenu", "PipboyMenu", "BarterMenu", "ContainerMenu" };
                                    const char* menuLabels[] = { _L("UI_MenuExamine"), _L("UI_MenuPipboy"), _L("UI_MenuBarter"), _L("UI_MenuContainer") };
                                    for (int m = 0; m < 4; ++m) {
                                        bool on = std::find(rule.menus.begin(), rule.menus.end(), menuKeys[m]) != rule.menus.end();
                                        if (ImGui::Checkbox(menuLabels[m], &on)) {
                                            if (on) rule.menus.push_back(menuKeys[m]);
                                            else rule.menus.erase(std::remove(rule.menus.begin(), rule.menus.end(), menuKeys[m]), rule.menus.end());

                                            // 四个标准菜单按固定顺序重排，用户手写的其它菜单名（如 CraftingMenu）保留在后面
                                            std::vector<std::string> ordered;
                                            for (const char* k : menuKeys) {
                                                if (std::find(rule.menus.begin(), rule.menus.end(), k) != rule.menus.end()) ordered.push_back(k);
                                            }
                                            for (const auto& name : rule.menus) {
                                                if (std::find(ordered.begin(), ordered.end(), name) == ordered.end()) ordered.push_back(name);
                                            }
                                            rule.menus = std::move(ordered);
                                            rule.hasMenus = !rule.menus.empty();
                                        }
                                        if (m < 3) ImGui::SameLine();
                                    }
                                    if (rule.hasMenus) {
                                        std::string extraMenus;
                                        for (const auto& name : rule.menus) {
                                            bool isStandard = false;
                                            for (const char* k : menuKeys) { if (name == k) { isStandard = true; break; } }
                                            if (!isStandard) extraMenus += (extraMenus.empty() ? "" : ", ") + name;
                                        }
                                        if (!extraMenus.empty()) ImGui::TextDisabled("+ %s", extraMenus.c_str());
                                    }
                                    ImGui::TextDisabled("%s", _L("UI_MenusHint"));
                                    ImGui::TreePop();
                                }

                                ImGui::Checkbox(_L("UI_BroadcastOnly"), &rule.broadcastOnly);
                                ImGui::TextDisabled("%s", _L("UI_BroadcastOnlyHint"));
                            }

                            if (rule.displayType == 0 || rule.displayType == 1) {
                                if (ImGui::CollapsingHeader(_L("UI_DataValueSettings"), ImGuiTreeNodeFlags_DefaultOpen)) {
                                    if (rule.displayType == 0) {
                                        bool hD = (rule.hideDifference == 1);
                                        if (ImGui::Checkbox(_L("UI_HideDifference"), &hD)) rule.hideDifference = hD ? 1 : 0;
                                        ImGui::SameLine();
                                        bool iD = (rule.invertDiffColor == 1);
                                        if (ImGui::Checkbox(_L("UI_InvertDiffColor"), &iD)) rule.invertDiffColor = iD ? 1 : 0;
                                    }
                                    InputTextString(_L("UI_ValueTextOverride"), rule.valueText);
                                    const char* states[] = { _L("UI_StateNormal"), _L("UI_StateStandard"), _L("UI_StateGood"), _L("UI_StateBad") };
                                    int curS = 0;
                                    if (rule.valueStandard) curS = 1; else if (rule.state == "good") curS = 2; else if (rule.state == "bad") curS = 3;
                                    if (ImGui::Combo(_L("UI_ValueColorState"), &curS, states, 4)) {
                                        rule.valueStandard = (curS == 1); rule.state = (curS == 2) ? "good" : (curS == 3 ? "bad" : "normal");
                                    }
                                    if (ImGui::TreeNodeEx("自定义数值颜色 (高级)", ImGuiTreeNodeFlags_None)) {
                                        ColorEdit3U32("覆盖基础颜色", &rule.valueColor);
                                        ImGui::TreePop();
                                    }
                                }
                            }

                            if (rule.displayType == 1) {
                                if (ImGui::CollapsingHeader(_L("UI_Type1_BarSettings"), ImGuiTreeNodeFlags_DefaultOpen)) {
                                    ImGui::Checkbox(_L("UI_ShowBar"), &rule.showBar); ImGui::SameLine();
                                    ImGui::Checkbox(_L("UI_ShowValue"), &rule.showValue);
                                    ImGui::SliderFloat(_L("UI_FillPct"), &rule.fillPct, 0.0f, 1.0f);
                                    ImGui::SliderFloat(_L("UI_ShieldPct"), &rule.shieldPct, 0.0f, 1.0f);
                                    ColorEdit3U32("进度条主颜色", &rule.fillColor);
                                }
                            }
                            else if (rule.displayType == 2) {
                                if (ImGui::CollapsingHeader(_L("UI_Type2_FlexSettings"), ImGuiTreeNodeFlags_DefaultOpen)) {
                                    ImGui::TextColored({ 0.4f, 0.8f, 0.4f, 1.0f }, _L("UI_BoxLeft"));
                                    InputTextString("左侧图标##L", rule.globalLeftBox.tag); ImGui::Checkbox("作为图标渲染##L", &rule.globalLeftBox.isIcon); InputTextString("显示数值##L", rule.globalLeftBox.value);
                                    ImGui::Separator();
                                    ImGui::TextColored({ 0.4f, 0.8f, 0.4f, 1.0f }, _L("UI_BoxRight"));
                                    InputTextString("右侧图标##R", rule.globalRightBox.tag); ImGui::Checkbox("作为图标渲染##R", &rule.globalRightBox.isIcon); InputTextString("显示数值##R", rule.globalRightBox.value);
                                }
                            }

                            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                            if (ImGui::Button("危险：永久删除此卡片 (Delete Rule)", ImVec2(-1, 30))) { ImGui::OpenPopup("DeleteRulePopup"); }
                            ImGui::PopStyleColor(2);

                            if (ImGui::BeginPopupModal("DeleteRulePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                                ImGui::Text("确定要永久删除卡片 [%s] 吗？\n警告：删除操作将立即生效，直接修改或抹除物理文件！", rule.id.c_str());
                                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                                if (ImGui::Button("确认删除", ImVec2(120, 30))) { pendingDeleteID = rule.id; ImGui::CloseCurrentPopup(); }
                                ImGui::SameLine();
                                if (ImGui::Button("取消", ImVec2(120, 30))) { ImGui::CloseCurrentPopup(); }
                                ImGui::EndPopup();
                            }
                        }
                    }
                }
                else {
                    ImGui::TextDisabled(_L("UI_SelectRulePrompt"));
                }
                ImGui::EndChild();
            }
            ImGui::End();

            // ==============================================================
            // 👑 核心修复 3: 移除了导致编译告警的 Emoji，确保不同环境不报 C4566
            // ==============================================================
            if (!selectedID.empty()) {
                ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(800, 1000));
                ImGui::Begin("Live Preview (实时排版预览)", nullptr);

                ImGui::TextDisabled("当前选中的卡片及挂载在它下方的所有子集：");
                ImGui::Spacing();

                std::unordered_set<std::string> previewVisited;

                std::function<void(const std::string&, int)> DrawPreviewNode = [&](const std::string& id, int type) {
                    if (previewVisited.count(id)) return;
                    previewVisited.insert(id);

                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float pW = ImGui::GetContentRegionAvail().x;
                    float pH = 34.0f;

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImU32 baseLineCol = IM_COL32(255, 255, 255, 60);
                    ImU32 themeTextCol = IM_COL32(26, 255, 128, 255);
                    ImU32 darkBg = IM_COL32(15, 15, 15, 200);

                    drawList->AddRectFilled(p, ImVec2(p.x + pW, p.y + pH), darkBg);

                    if (type == 1 || type == 2) {
                        std::string titleStr;
                        if (type == 1) titleStr = "[C++] " + id;
                        else {
                            std::string vLabel = id;
                            for (auto& v : masterVanillaList) { if (v.id == id) { vLabel = v.label; break; } }
                            titleStr = vLabel;
                        }

                        float textY = p.y + (pH - ImGui::GetTextLineHeight()) * 0.5f;
                        drawList->AddText(ImVec2(p.x + 10.0f, textY), themeTextCol, titleStr.c_str());

                        std::string valStr = (id == "$ammo") ? "037/000" : ((id == "$dmg") ? "124" : "---");
                        float valW = ImGui::CalcTextSize(valStr.c_str()).x;
                        float vX = p.x + pW - valW - 10.0f;
                        drawList->AddText(ImVec2(vX, textY), themeTextCol, valStr.c_str());
                        drawList->AddLine(ImVec2(vX - 6.0f, p.y + 4.0f), ImVec2(vX - 6.0f, p.y + pH - 4.0f), baseLineCol, 1.0f);
                    }
                    else if (type == 0) {
                        auto it = std::find_if(rules.begin(), rules.end(), [&](const auto& r) {return r.id == id; });
                        if (it != rules.end()) {
                            auto& rule = *it;
                            ImU32 bgCol = rule.hasBackground ? IM_COL32((rule.backgroundColor >> 16) & 0xFF, (rule.backgroundColor >> 8) & 0xFF, rule.backgroundColor & 0xFF, 140) : 0;
                            if (rule.hasBackground) drawList->AddRectFilled(p, ImVec2(p.x + pW, p.y + pH), bgCol);

                            float textY = p.y + (pH - ImGui::GetTextLineHeight()) * 0.5f;

                            if (rule.displayType == 0) {
                                ImU32 titleCol = rule.highlightLabel ? IM_COL32(255, 255, 255, 255) : themeTextCol;
                                std::string titleStr = rule.titleText.empty() ? "属性文本" : rule.titleText;
                                drawList->AddText(ImVec2(p.x + 10.0f, textY), titleCol, titleStr.c_str());

                                ImU32 valCol = themeTextCol;
                                if (rule.valueColor != 0) valCol = IM_COL32((rule.valueColor >> 16) & 0xFF, (rule.valueColor >> 8) & 0xFF, rule.valueColor & 0xFF, 255);
                                else if (rule.state == "good") valCol = IM_COL32(100, 255, 100, 255);
                                else if (rule.state == "bad") valCol = IM_COL32(255, 100, 100, 255);
                                else if (rule.valueStandard) valCol = IM_COL32(150, 150, 150, 255);

                                std::string valStr = rule.valueText.empty() ? "256" : rule.valueText;
                                float valW = ImGui::CalcTextSize(valStr.c_str()).x;
                                float vX = p.x + pW - valW - 10.0f;
                                drawList->AddText(ImVec2(vX, textY), valCol, valStr.c_str());
                                drawList->AddLine(ImVec2(vX - 6.0f, p.y + 4.0f), ImVec2(vX - 6.0f, p.y + pH - 4.0f), baseLineCol, 1.0f);
                            }
                            else if (rule.displayType == 1) {
                                ImU32 titleCol = rule.highlightLabel ? IM_COL32(255, 255, 255, 255) : themeTextCol;
                                std::string titleStr = rule.titleText.empty() ? "进度属性" : rule.titleText;
                                drawList->AddText(ImVec2(p.x + 10.0f, textY), titleCol, titleStr.c_str());

                                float barW = pW * 0.35f;
                                float barX = p.x + pW - barW - 10.0f;
                                float textStartX = barX;

                                if (rule.showBar) {
                                    float barH = 20.0f;
                                    float barY = p.y + (pH - barH) * 0.5f;
                                    drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(255, 255, 255, 100), 0.0f, 0, 2.0f);
                                    drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(255, 255, 255, 20));

                                    float fillX = barX + 5.0f; float fillY = barY + 4.0f;
                                    float maxFillW = barW - 10.0f; float fillH = barH - 8.0f;

                                    ImU32 fCol = IM_COL32((rule.fillColor >> 16) & 0xFF, (rule.fillColor >> 8) & 0xFF, rule.fillColor & 0xFF, 255);
                                    float pct = rule.fillPct < 0 ? 0.65f : rule.fillPct;
                                    float drawW = maxFillW * pct;
                                    if (drawW > 0) drawList->AddRectFilled(ImVec2(fillX, fillY), ImVec2(fillX + drawW, fillY + fillH), fCol);

                                    float shieldPct = rule.shieldPct;
                                    if (shieldPct > 0) {
                                        float shieldW = maxFillW * std::min(1.0f, shieldPct);
                                        float shieldH = fillH * 0.33f;
                                        float shieldYOffset = fillY + (fillH - shieldH) * 0.5f;
                                        float shieldXOffset = fillX + maxFillW - shieldW;
                                        drawList->AddRectFilled(ImVec2(shieldXOffset, shieldYOffset), ImVec2(shieldXOffset + shieldW, shieldYOffset + shieldH), IM_COL32(255, 255, 255, 180));
                                    }
                                }

                                if (rule.showValue) {
                                    std::string valStr = rule.valueText.empty() ? "65/100" : rule.valueText;
                                    float valW = ImGui::CalcTextSize(valStr.c_str()).x;
                                    ImU32 valCol = themeTextCol;
                                    if (rule.valueColor != 0) valCol = IM_COL32((rule.valueColor >> 16) & 0xFF, (rule.valueColor >> 8) & 0xFF, rule.valueColor & 0xFF, 255);
                                    else if (rule.state == "good") valCol = IM_COL32(100, 255, 100, 255);
                                    else if (rule.state == "bad") valCol = IM_COL32(255, 100, 100, 255);
                                    else if (rule.valueStandard) valCol = IM_COL32(150, 150, 150, 255);

                                    textStartX = barX - valW - 10.0f;
                                    drawList->AddText(ImVec2(textStartX, textY), valCol, valStr.c_str());
                                }
                                drawList->AddLine(ImVec2(textStartX - 6.0f, p.y + 4.0f), ImVec2(textStartX - 6.0f, p.y + pH - 4.0f), baseLineCol, 1.0f);
                            }
                            else if (rule.displayType == 2) {
                                float iconSize = 18.0f; float iconPad = 22.0f;
                                float lX = p.x + 10.0f;
                                if (rule.globalLeftBox.isIcon) {
                                    drawList->AddRectFilled(ImVec2(lX, textY), ImVec2(lX + iconSize, textY + iconSize), IM_COL32(150, 150, 150, 255));
                                    std::string lval = rule.globalLeftBox.value.empty() ? "L.Val" : rule.globalLeftBox.value;
                                    drawList->AddText(ImVec2(lX + iconPad, textY), themeTextCol, lval.c_str());
                                }
                                else {
                                    std::string lStr = rule.globalLeftBox.tag + " " + (rule.globalLeftBox.value.empty() ? "L.Val" : rule.globalLeftBox.value);
                                    drawList->AddText(ImVec2(lX, textY), themeTextCol, lStr.c_str());
                                }

                                std::string rVal = rule.globalRightBox.value.empty() ? "R.Val" : rule.globalRightBox.value;
                                float rW = ImGui::CalcTextSize(rVal.c_str()).x;
                                float rX = p.x + pW - 10.0f;

                                if (rule.globalRightBox.isIcon) {
                                    drawList->AddText(ImVec2(rX - rW, textY), themeTextCol, rVal.c_str());
                                    drawList->AddRectFilled(ImVec2(rX - rW - iconPad, textY), ImVec2(rX - rW - iconPad + iconSize, textY + iconSize), IM_COL32(150, 150, 150, 255));
                                }
                                else {
                                    std::string rStr = rule.globalRightBox.tag + " " + rVal;
                                    float fullRW = ImGui::CalcTextSize(rStr.c_str()).x;
                                    drawList->AddText(ImVec2(rX - fullRW, textY), themeTextCol, rStr.c_str());
                                }
                            }
                        }
                    }

                    drawList->AddLine(ImVec2(p.x, p.y + pH), ImVec2(p.x + pW, p.y + pH), IM_COL32(255, 255, 255, 20), 1.0f);
                    ImGui::Dummy(ImVec2(pW, pH));

                    if (childrenMap.count(id)) {
                        for (auto* childProxy : childrenMap[id]) DrawPreviewNode(childProxy->id, childProxy->type);
                    }
                    };

                DrawPreviewNode(selectedID, selectedType);
                ImGui::End();
            }
        }

            ImGui::Render();
        }

        if (m_isVisible && m_pContext) {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }

    HRESULT WINAPI ImGuiManager::Present_Hook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        using Present_t = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
        RenderCore(pSwapChain);
        auto original = reinterpret_cast<Present_t>(m_originalPresentVtEntry);
        return original(pSwapChain, SyncInterval, Flags);
    }

    LRESULT WINAPI ImGuiManager::WndProc_Hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_KEYDOWN) {
            const auto key = static_cast<UINT>(wParam);
            const bool firstPress = (lParam & 0x40000000) == 0;
            if (firstPress && g_captureToggleKey && m_isVisible) {
                if (key == VK_ESCAPE) {
                    g_captureToggleKey = false;
                    return true;
                }
                if (!IsModifierKey(key)) {
                    g_toggleKey = key;
                    g_toggleModifiers = CurrentWin32ModifierMask();
                    g_captureToggleKey = false;
                    SaveEditorSettings();
                    REX::INFO("[IIF] ImGui editor toggle hotkey changed to {}", ShortcutLabel());
                    return true;
                }
            }
            if (firstPress && IsToggleShortcut(key)) {
                REX::INFO("[IIF] ImGui editor toggle hotkey pressed: {}", ShortcutLabel());
                GetSingleton().ToggleDisplay();
                return true;
            }
        }
        if (m_isInitialized && m_isVisible) {
            if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) return true;
            if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) return true;
        }
        return CallWindowProc(m_originalWndProc, hWnd, uMsg, wParam, lParam);
    }
}
