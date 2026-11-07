#include "pch.h"
#include "JsonReader.h"
#include "UIHooks.h" 
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>

namespace IIF::JsonReader {
    std::vector<JsonRule> g_rules;
    std::map<std::string, CPPOverride> g_cppOverrides;

    using json = nlohmann::json;

    bool ToBoolSafe(const json& v, bool defaultVal = false) {
        if (v.is_boolean()) return v.get<bool>();
        if (v.is_number_integer()) return v.get<int>() > 0;
        if (v.is_number_float()) return v.get<double>() > 0.0;
        if (v.is_string()) {
            std::string s = v.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return (s == "true" || s == "1");
        }
        return defaultVal;
    }

    bool ParseBoolSafe(const json& j, const std::string& key, bool defaultVal = false) {
        if (!j.contains(key)) return defaultVal;
        return ToBoolSafe(j[key], defaultVal);
    }

    int ParseTriStateSafe(const json& j, const std::string& key) {
        if (!j.contains(key)) return -1;
        if (j[key].is_boolean()) return j[key].get<bool>() ? 1 : 0;
        if (j[key].is_number_integer()) return j[key].get<int>() > 0 ? 1 : 0;
        if (j[key].is_string()) {
            std::string s = j[key].get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return (s == "true" || s == "1") ? 1 : 0;
        }
        return -1;
    }

    // 最终显示数值的小数位数：-1 / 缺省 = 自动，0~6 = 固定位数
    int ParseRoundDigitsSafe(const json& j, const std::string& key, int defaultVal = -1) {
        if (!j.contains(key)) return defaultVal;
        const auto& v = j[key];
        int digits = defaultVal;
        if (v.is_number_integer()) digits = v.get<int>();
        else if (v.is_number_float()) digits = static_cast<int>(std::round(v.get<double>()));
        else if (v.is_string()) {
            try { digits = std::stoi(v.get<std::string>()); }
            catch (...) { return defaultVal; }
        }
        else return defaultVal;
        if (digits < -1) return -1;
        return (std::min)(digits, 6);
    }

    // 把 "ExamineMenu" / "examine" / "Examine" 统一成 "examine"，便于宽松匹配
    std::string NormalizeMenuToken(std::string a_text) {
        std::string out;
        for (char c : a_text) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc)) out.push_back(static_cast<char>(std::tolower(uc)));
        }
        if (out.size() > 4 && out.compare(out.size() - 4, 4, "menu") == 0) out.resize(out.size() - 4);
        return out;
    }

    // 简写 -> 真实菜单名；无法识别的名字原样返回，方便直接写 "CraftingMenu" 之类的菜单
    std::string CanonicalMenuName(const std::string& a_token) {
        const std::string t = NormalizeMenuToken(a_token);
        if (t == "examine") return "ExamineMenu";
        if (t == "pipboy") return "PipboyMenu";
        if (t == "barter") return "BarterMenu";
        if (t == "container") return "ContainerMenu";
        return a_token;
    }

    // 菜单白名单：支持 ["ExamineMenu"] 数组、{"ExamineMenu": true} 对象、单个字符串三种写法。
    // 缺省或空列表 = 不限制菜单。
    void ParseMenusSafe(const json& j, const std::string& key, std::vector<std::string>& out, bool& has) {
        if (!j.contains(key)) return;
        const auto& v = j[key];
        std::vector<std::string> parsed;
        if (v.is_array()) {
            for (const auto& item : v) if (item.is_string()) parsed.push_back(item.get<std::string>());
        }
        else if (v.is_object()) {
            for (auto& el : v.items()) if (ToBoolSafe(el.value(), false)) parsed.push_back(el.key());
        }
        else if (v.is_string()) {
            parsed.push_back(v.get<std::string>());
        }
        if (parsed.empty()) return;

        // 存成真实菜单名，运行时直接交给 UI::GetMenuOpen 查询
        out.clear();
        for (auto& name : parsed) {
            const std::string canonical = CanonicalMenuName(name);
            if (std::find(out.begin(), out.end(), canonical) == out.end()) out.push_back(canonical);
        }
        has = !out.empty();
    }

    std::uint32_t ParseHexColor(const json& j) {
        // 🛡️ 全面防护：处理字符串和数字两种输入，不合法的返回0
        if (j.is_string()) {
            std::string s = j.get<std::string>();
            try {
                if (s.find("0x") == 0 || s.find("0X") == 0) return std::stoul(s, nullptr, 16);
                if (s.find("#") == 0) return std::stoul(s.substr(1), nullptr, 16);
            }
            catch (...) { return 0; }
        }
        else if (j.is_number_unsigned()) {
            return j.get<std::uint32_t>();
        }
        return 0;
    }

    std::string ToHexColor(std::uint32_t color) {
        char hex[12];
        sprintf_s(hex, "0x%06X", color & 0xFFFFFF);
        return std::string(hex);
    }

    json SerializeDataSource(const DataSource& source)
    {
        json ds;
        ds["Type"] = source.Type;
        ds["ID"] = source.ID;
        ds["Required"] = source.Required;
        ds["Offset"] = source.Offset;
        ds["Multiplier"] = source.Multiplier;
        ds["Suffix"] = source.Suffix;
        if (source.roundDigits >= 0) ds["roundDigits"] = source.roundDigits;
        return ds;
    }

    json SerializeBoxConfig(const BoxConfig& box)
    {
        json j;
        j["active"] = box.active;
        j["tag"] = box.tag;
        j["isIcon"] = box.isIcon;
        j["value"] = box.value;
        j["state"] = box.state;
        j["align"] = box.align;
        if (box.dataSource.active) {
            j["DataSource"] = SerializeDataSource(box.dataSource);
        }
        return j;
    }

    json SerializeResultBlock(const ResultBlock& result)
    {
        json j;
        if (!result.value.empty()) j["value"] = result.value;
        if (!result.tag.empty()) j["tag"] = result.tag;
        if (!result.state.empty()) j["state"] = result.state;
        if (!result.align.empty()) j["align"] = result.align;

        if (result.hideDifference != -1) j["hideDifference"] = (result.hideDifference == 1);
        if (result.invertDiffColor != -1) j["invertDiffColor"] = (result.invertDiffColor == 1);
        j["isIcon"] = result.isIcon;
        if (result.fillPct != -1.0f) j["fillPct"] = result.fillPct;
        if (result.shieldPct != 0.0f) j["shieldPct"] = result.shieldPct;
        if (result.fillColor != 0) j["fillColor"] = ToHexColor(result.fillColor);
        if (result.showBar != -1) j["showBar"] = (result.showBar == 1);
        if (result.showValue != -1) j["showValue"] = (result.showValue == 1);
        if (!result.valueText.empty()) j["valueText"] = result.valueText;
        if (!result.valueAlign.empty()) j["valueAlign"] = result.valueAlign;
        if (result.valueColor != 0) j["valueColor"] = ToHexColor(result.valueColor);
        if (result.valueStandard != -1) j["valueStandard"] = (result.valueStandard == 1);
        if (result.dataSource.active) {
            j["DataSource"] = SerializeDataSource(result.dataSource);
        }

        if (result.leftBox.active && (!result.leftBox.tag.empty() || !result.leftBox.value.empty() || result.leftBox.isIcon || result.leftBox.state != "normal" || !result.leftBox.align.empty() || result.leftBox.dataSource.active)) {
            j["leftBox"] = SerializeBoxConfig(result.leftBox);
        }
        if (result.rightBox.active && (!result.rightBox.tag.empty() || !result.rightBox.value.empty() || result.rightBox.isIcon || result.rightBox.state != "normal" || !result.rightBox.align.empty() || result.rightBox.dataSource.active)) {
            j["rightBox"] = SerializeBoxConfig(result.rightBox);
        }

        if (j.is_null() || j.empty()) j["value"] = "";
        return j;
    }

    ResultBlock MergeRuleToResult(const JsonRule& rule)
    {
        ResultBlock merged = rule.result;

        if (!rule.valueText.empty()) merged.value = rule.valueText;
        if (!rule.state.empty()) merged.state = rule.state;
        if (rule.hideDifference != -1) merged.hideDifference = rule.hideDifference;
        if (rule.invertDiffColor != -1) merged.invertDiffColor = rule.invertDiffColor;
        merged.showBar = rule.showBar ? 1 : 0;
        merged.showValue = rule.showValue ? 1 : 0;
        if (rule.fillPct != -1.0f) merged.fillPct = rule.fillPct;
        merged.shieldPct = rule.shieldPct;
        if (rule.fillColor != 0) merged.fillColor = rule.fillColor;
        if (!rule.valueAlign.empty()) merged.valueAlign = rule.valueAlign;
        if (rule.valueColor != 0) merged.valueColor = rule.valueColor;
        if (rule.valueStandard) merged.valueStandard = 1;
        else merged.valueStandard = 0;
        if (rule.valueText.empty()) merged.valueText.clear();
        else merged.valueText = rule.valueText;

        merged.leftBox = rule.globalLeftBox;
        merged.rightBox = rule.globalRightBox;
        return merged;
    }

    json SerializeRuleForSave(const JsonRule& rule)
    {
        json outRule;
        if (!rule.id.empty()) outRule["id"] = rule.id;
        if (!rule.conditionType.empty()) outRule["ConditionType"] = rule.conditionType;
        if (!rule.matchType.empty()) outRule["MatchType"] = rule.matchType;
        if (!rule.conditionIDs.empty()) outRule["ConditionIDs"] = rule.conditionIDs;
        if (rule.hasMenus && !rule.menus.empty()) outRule["menus"] = rule.menus;
        if (rule.roundDigits >= 0) outRule["roundDigits"] = rule.roundDigits;
        outRule["broadcastOnly"] = rule.broadcastOnly;
        if (rule.inlineDamage) {
            outRule["inlineDamage"] = true;
            outRule["inlineDamageFormat"] = rule.inlineDamageFormat;
            if (rule.inlineDamageDigits >= 0) outRule["inlineDamageDigits"] = rule.inlineDamageDigits;
        }
        if (!rule.valuesMapping.empty()) {
            json mapping = json::object();
            for (const auto& pair : rule.valuesMapping) {
                mapping[pair.first] = SerializeResultBlock(pair.second);
            }
            outRule["ValuesMapping"] = mapping;
        }
        outRule["Result"] = SerializeResultBlock(MergeRuleToResult(rule));
        return outRule;
    }

    std::string NormalizeRulePath(const std::string& path)
    {
        if (path.empty()) return "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\gui_editor_rules.json";

        if (path.find(':') != std::string::npos) {
            auto fileName = std::filesystem::path(path).filename().string();
            if (!fileName.empty()) return std::string("Data\\F4SE\\Plugins\\ItemIntegrationFramework\\") + fileName;
        }

        if (path.find("Data\\F4SE\\Plugins\\ItemIntegrationFramework\\") == 0 ||
            path.find("Data/F4SE/Plugins/ItemIntegrationFramework/") == 0) {
            return path;
        }

        if (path.find("Data\\F4SE\\") == 0 || path.find("Data/F4SE/") == 0) {
            return path;
        }

        if (path.size() > 2 && (path[0] == '/' || path[0] == '\\')) {
            return std::string("Data\\F4SE\\Plugins\\ItemIntegrationFramework\\") + path.substr(1);
        }

        return std::string("Data\\F4SE\\Plugins\\ItemIntegrationFramework\\") + path;
    }

    void WriteGlobalDefaults(json& j, const JsonRule& rule)
    {
        if (!j.contains("global") || !j["global"].is_object()) j["global"] = json::object();
        auto& g = j["global"];

        g["id"] = rule.id;
        g["priority"] = rule.priority;
        g["displayType"] = rule.displayType;
        g["hasBackground"] = (rule.hasBackground == true);

        char hex[12];
        sprintf_s(hex, "0x%06X", rule.backgroundColor);
        g["backgroundColor"] = std::string(hex);

        if (!g.contains("content") || !g["content"].is_object()) g["content"] = json::object();
        if (!g["content"].contains("title") || !g["content"]["title"].is_object()) g["content"]["title"] = json::object();

        g["content"]["title"]["text"] = rule.titleText;
        g["content"]["title"]["highlight"] = (rule.highlightLabel == true);
        g["content"]["hasBackground"] = (rule.hasBackground == true);
        g["content"]["valueText"] = rule.valueText;

        if (rule.hasMenus && !rule.menus.empty()) g["menus"] = rule.menus;
        else g.erase("menus");

        if (rule.roundDigits >= 0) g["roundDigits"] = rule.roundDigits;
        else g.erase("roundDigits");

        // 文件级默认值；每条规则都会显式保存自己的 broadcastOnly，重载时不会被这里覆盖。
        g["broadcastOnly"] = (rule.broadcastOnly == true);

        if (rule.inlineDamage) {
            g["inlineDamage"] = true;
            g["inlineDamageFormat"] = rule.inlineDamageFormat;
            if (rule.inlineDamageDigits >= 0) g["inlineDamageDigits"] = rule.inlineDamageDigits;
            else g.erase("inlineDamageDigits");
        }
        else {
            g.erase("inlineDamage");
            g.erase("inlineDamageFormat");
            g.erase("inlineDamageDigits");
        }

        if (!rule.anchorTargets.empty()) {
            if (rule.anchorTargets.size() > 1) {
                g["anchorTarget"] = rule.anchorTargets;
                g["anchorMode"] = rule.anchorModes;
            }
            else {
                g["anchorTarget"] = rule.anchorTargets[0];
                if (!rule.anchorModes.empty()) g["anchorMode"] = rule.anchorModes[0];
            }
        }
        else {
            g.erase("anchorTarget");
            g.erase("anchorMode");
        }
    }

    RE::TESForm* GetFormFromIdentifier(const std::string& identifier) {
        auto pos = identifier.find('|');
        if (pos != std::string::npos) {
            std::string modName = identifier.substr(0, pos);
            std::string idStr = identifier.substr(pos + 1);
            try {
                RE::TESFormID localId = std::stoul(idStr, nullptr, 16);
                auto dataHandler = RE::TESDataHandler::GetSingleton();
                if (dataHandler) return dataHandler->LookupForm(localId, modName);
            }
            catch (...) {}
        }
        return nullptr;
    }

    float GetItemActorValue(RE::TESForm* a_form, RE::TBO_InstanceData* a_instance, RE::TESForm* a_avForm) {
        if (!a_form || !a_avForm) return 0.0f;
        auto boundObj = a_form->As<RE::TESBoundObject>();
        if (!boundObj) return 0.0f;

        if (boundObj->Is(RE::ENUM_FORM_ID::kWEAP)) {
            auto weap = boundObj->As<RE::TESObjectWEAP>();
            // 🛡️ 防护：As<> 在极端情况下可能返回 null
            if (!weap) return 0.0f;
            auto inst = a_instance ? static_cast<RE::TESObjectWEAP::InstanceData*>(a_instance) : &weap->weaponData;
            // 🛡️ SEH防护：actorValues 指针可能因 static_cast 类型不匹配读取到错误的偏移量
            if (inst) {
                __try {
                    if (inst->actorValues) {
                        for (auto& tuple : *inst->actorValues) {
                            if (tuple.first == a_avForm) return *reinterpret_cast<float*>(&tuple.second);
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        else if (boundObj->Is(RE::ENUM_FORM_ID::kARMO)) {
            auto armo = boundObj->As<RE::TESObjectARMO>();
            // 🛡️ 防护：As<> 在极端情况下可能返回 null
            if (!armo) return 0.0f;
            auto inst = a_instance ? static_cast<RE::TESObjectARMO::InstanceData*>(a_instance) : &armo->armorData;
            // 🛡️ SEH防护：actorValues 指针可能因 static_cast 类型不匹配读取到错误的偏移量
            if (inst) {
                __try {
                    if (inst->actorValues) {
                        for (auto& tuple : *inst->actorValues) {
                            if (tuple.first == a_avForm) return *reinterpret_cast<float*>(&tuple.second);
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        return 0.0f;
    }

    // 从武器实例数据里读取一次发射的弹丸总数（RangedData::numProjectiles）。
    // 武器实例已包含 OMOD 修改，因此霰弹枪的多弹丸枪管等会体现在这里。
    bool ReadWeaponNumProjectiles(RE::TESForm* a_form, RE::TBO_InstanceData* a_instance, float& a_out) {
        if (!a_form || !a_form->Is(RE::ENUM_FORM_ID::kWEAP)) return false;
        auto weap = a_form->As<RE::TESObjectWEAP>();
        if (!weap) return false;
        auto inst = a_instance ? static_cast<RE::TESObjectWEAP::InstanceData*>(a_instance) : &weap->weaponData;
        if (!inst) return false;

        // 🛡️ SEH防护：instance 指针可能来自已被卸载 Mod 的损坏数据
        __try {
            if (!inst->rangedData) return false;
            const int count = static_cast<int>(inst->rangedData->numProjectiles);
            if (count < 0) return false;
            a_out = static_cast<float>((std::max)(count, 1));
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool IsNumProjectilesSource(const DataSource& a_source) {
        return a_source.active && a_source.Type == "WeaponStat" && a_source.ID == "NumProjectiles";
    }

    // 只有数据源确实是 NumProjectiles 时才允许把数值并进原版伤害行，
    // 避免误给其它规则打开 inlineDamage 后反而吞掉原本的卡片。
    bool RuleUsesNumProjectiles(const JsonRule& a_rule, const ResultBlock& a_result) {
        if (a_result.rightBox.active && a_result.rightBox.dataSource.active) return IsNumProjectilesSource(a_result.rightBox.dataSource);
        if (a_result.leftBox.active && a_result.leftBox.dataSource.active) return IsNumProjectilesSource(a_result.leftBox.dataSource);
        if (a_result.dataSource.active) return IsNumProjectilesSource(a_result.dataSource);
        if (a_rule.globalRightBox.active && a_rule.globalRightBox.dataSource.active) return IsNumProjectilesSource(a_rule.globalRightBox.dataSource);
        if (a_rule.globalLeftBox.active && a_rule.globalLeftBox.dataSource.active) return IsNumProjectilesSource(a_rule.globalLeftBox.dataSource);
        return false;
    }

    // 🛡️ 主动调用 BGSEntryPoint::HandleEntryPoint，让玩家身上所有相关 perk 把修正叠加到 baseValue 上
    // 与 main.cpp 中的 hook 签名保持一致，确保 ABI 匹配
    float ApplyPerkModifier(std::uint32_t a_entryPoint, float a_baseValue, RE::TESObjectWEAP* a_weapon) {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return a_baseValue;

        float val = a_baseValue;
        __try {
            using HandleEntryPoint_t = void(*)(std::uint32_t, RE::Actor*, void*, void*, void*, void*, void*, void*);
            REL::Relocation<HandleEntryPoint_t> func{ RE::ID::BGSEntryPoint::HandleEntryPoint };
            func(a_entryPoint, player, &val, a_weapon, nullptr, nullptr, nullptr, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return a_baseValue;
        }
        return val;
    }

    std::string FormatValue(float val, int roundDigits = -1) {
        char buf[64];
        if (roundDigits < 0) {
            if (std::abs(val - std::round(val)) < 0.001f) snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(val)));
            else snprintf(buf, sizeof(buf), "%.1f", val);
        }
        else {
            snprintf(buf, sizeof(buf), "%.*f", (std::min)(roundDigits, 6), static_cast<double>(val));
        }
        return std::string(buf);
    }

    double RoundToDigits(double val, int roundDigits) {
        if (roundDigits < 0) return val;
        const double scale = std::pow(10.0, static_cast<double>((std::min)(roundDigits, 6)));
        return std::round(val * scale) / scale;
    }

    // hasMenus == false 时所有菜单都显示；否则只要列出的菜单中有任意一个正开着就显示
    bool IsRuleVisibleInMenu(const JsonRule& a_rule) {
        if (!a_rule.hasMenus || a_rule.menus.empty()) return true;

        auto* ui = RE::UI::GetSingleton();
        if (!ui) return false;
        for (const auto& name : a_rule.menus) {
            if (!name.empty() && ui->GetMenuOpen(name.c_str())) return true;
        }
        return false;
    }

    void ParseBox(const json& j, BoxConfig& box) {
        if (j.contains("tag")) box.tag = j["tag"].get<std::string>();
        box.isIcon = ParseBoolSafe(j, "isIcon", false);
        if (j.contains("value")) box.value = j["value"].get<std::string>();
        if (j.contains("state")) box.state = j["state"].get<std::string>();
        if (j.contains("align")) box.align = j["align"].get<std::string>();
        if (j.contains("DataSource")) {
            box.dataSource.active = true;
            if (j["DataSource"].contains("Type")) box.dataSource.Type = j["DataSource"]["Type"].get<std::string>();
            if (j["DataSource"].contains("ID")) box.dataSource.ID = j["DataSource"]["ID"].get<std::string>();
            if (j["DataSource"].contains("Required")) box.dataSource.Required = j["DataSource"]["Required"].get<int>();
            if (j["DataSource"].contains("Offset")) box.dataSource.Offset = j["DataSource"]["Offset"].get<float>();
            if (j["DataSource"].contains("Multiplier")) box.dataSource.Multiplier = j["DataSource"]["Multiplier"].get<float>();
            if (j["DataSource"].contains("Suffix")) box.dataSource.Suffix = j["DataSource"]["Suffix"].get<std::string>();
            box.dataSource.roundDigits = ParseRoundDigitsSafe(j["DataSource"], "roundDigits", box.dataSource.roundDigits);
        }
        box.active = true;
    }

    void ParseResultBlock(const json& j, ResultBlock& res) {
        res.hasContent = true;
        if (j.contains("value")) res.value = j["value"].get<std::string>();
        if (j.contains("tag")) res.tag = j["tag"].get<std::string>();
        if (j.contains("state")) res.state = j["state"].get<std::string>();
        if (j.contains("align")) res.align = j["align"].get<std::string>();

        res.hideDifference = ParseTriStateSafe(j, "hideDifference");
        res.invertDiffColor = ParseTriStateSafe(j, "invertDiffColor");
        res.isIcon = ParseBoolSafe(j, "isIcon", false);

        if (j.contains("DataSource")) {
            res.dataSource.active = true;
            if (j["DataSource"].contains("Type")) res.dataSource.Type = j["DataSource"]["Type"].get<std::string>();
            if (j["DataSource"].contains("ID")) res.dataSource.ID = j["DataSource"]["ID"].get<std::string>();
            if (j["DataSource"].contains("Required")) res.dataSource.Required = j["DataSource"]["Required"].get<int>();
            if (j["DataSource"].contains("Offset")) res.dataSource.Offset = j["DataSource"]["Offset"].get<float>();
            if (j["DataSource"].contains("Multiplier")) res.dataSource.Multiplier = j["DataSource"]["Multiplier"].get<float>();
            if (j["DataSource"].contains("Suffix")) res.dataSource.Suffix = j["DataSource"]["Suffix"].get<std::string>();
            res.dataSource.roundDigits = ParseRoundDigitsSafe(j["DataSource"], "roundDigits", res.dataSource.roundDigits);
        }
        if (j.contains("fillPct")) res.fillPct = j["fillPct"].get<float>();
        if (j.contains("shieldPct")) res.shieldPct = j["shieldPct"].get<float>();
        if (j.contains("fillColor")) res.fillColor = ParseHexColor(j["fillColor"]);

        res.showBar = ParseTriStateSafe(j, "showBar");
        res.showValue = ParseTriStateSafe(j, "showValue");

        if (j.contains("valueText")) res.valueText = j["valueText"].get<std::string>();
        if (j.contains("valueAlign")) res.valueAlign = j["valueAlign"].get<std::string>();
        if (j.contains("valueColor")) res.valueColor = ParseHexColor(j["valueColor"]);

        res.valueStandard = ParseTriStateSafe(j, "valueStandard");

        if (j.contains("leftBox")) ParseBox(j["leftBox"], res.leftBox);
        if (j.contains("rightBox")) ParseBox(j["rightBox"], res.rightBox);
    }

    void LoadConfigs() {
        g_rules.clear();
        std::string configDir = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\";
        if (!std::filesystem::exists(configDir)) { std::filesystem::create_directories(configDir); return; }

        int fileCount = 0;

        // 🛡️ TOCTOU 防护：directory_iterator 可能在构造时因目录被删除/权限变更而抛出 filesystem_error
        try {
            for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
                if (entry.path().extension() == ".json" || entry.path().extension() == ".jsonc") {
                    if (entry.path().filename() == "CPP_Overrides.json") continue;

                    try {
                        std::ifstream file(entry.path());
                        json j = json::parse(file, nullptr, true, true);

                        fileCount++;

                        JsonRule baseTemplate;
                        baseTemplate.originPath = entry.path().string();

                        if (j.contains("global")) {
                            auto& gl = j["global"];
                            if (gl.contains("id")) baseTemplate.id = gl["id"].get<std::string>();
                            if (gl.contains("priority")) baseTemplate.priority = gl["priority"].get<int>();
                            if (gl.contains("displayType")) baseTemplate.displayType = gl["displayType"].get<int>();

                            baseTemplate.hasBackground = ParseBoolSafe(gl, "hasBackground", false);
                            if (gl.contains("backgroundColor")) baseTemplate.backgroundColor = ParseHexColor(gl["backgroundColor"]);

                            if (gl.contains("anchorTarget")) {
                                if (gl["anchorTarget"].is_array()) {
                                    for (auto& tg : gl["anchorTarget"]) baseTemplate.anchorTargets.push_back(tg.get<std::string>());
                                }
                                else {
                                    baseTemplate.anchorTargets.push_back(gl["anchorTarget"].get<std::string>());
                                }
                            }
                            if (gl.contains("anchorMode")) {
                                if (gl["anchorMode"].is_array()) {
                                    for (auto& md : gl["anchorMode"]) baseTemplate.anchorModes.push_back(md.get<std::string>());
                                }
                                else {
                                    baseTemplate.anchorModes.push_back(gl["anchorMode"].get<std::string>());
                                }
                            }

                            if (gl.contains("content")) {
                                auto& cnt = gl["content"];
                                if (cnt.contains("title")) {
                                    if (cnt["title"].contains("text")) baseTemplate.titleText = cnt["title"]["text"].get<std::string>();
                                    baseTemplate.highlightLabel = ParseBoolSafe(cnt["title"], "highlight", false);
                                }
                                if (cnt.contains("valueText")) baseTemplate.valueText = cnt["valueText"].get<std::string>();
                                if (cnt.contains("hasBackground")) {
                                    baseTemplate.hasBackground = ParseBoolSafe(cnt, "hasBackground", baseTemplate.hasBackground);
                                }
                            }

                            ParseMenusSafe(gl, "menus", baseTemplate.menus, baseTemplate.hasMenus);
                            baseTemplate.roundDigits = ParseRoundDigitsSafe(gl, "roundDigits", baseTemplate.roundDigits);
                            baseTemplate.broadcastOnly = ParseBoolSafe(gl, "broadcastOnly", baseTemplate.broadcastOnly);
                            baseTemplate.inlineDamage = ParseBoolSafe(gl, "inlineDamage", baseTemplate.inlineDamage);
                            if (gl.contains("inlineDamageFormat") && gl["inlineDamageFormat"].is_string()) {
                                baseTemplate.inlineDamageFormat = gl["inlineDamageFormat"].get<std::string>();
                            }
                            baseTemplate.inlineDamageDigits = ParseRoundDigitsSafe(gl, "inlineDamageDigits", baseTemplate.inlineDamageDigits);
                        }

                        if (j.contains("DataRules") && j["DataRules"].is_array()) {
                            for (const auto& ruleJson : j["DataRules"]) {
                                JsonRule rule = baseTemplate;
                                if (ruleJson.contains("id")) rule.id = ruleJson["id"].get<std::string>();
                                if (ruleJson.contains("ConditionType")) rule.conditionType = ruleJson["ConditionType"].get<std::string>();
                                if (ruleJson.contains("MatchType")) rule.matchType = ruleJson["MatchType"].get<std::string>();
                                // 🛡️ 防护：ConditionIDs 必须是数组，误写为字符串时跳过以免异常
                                if (ruleJson.contains("ConditionIDs") && ruleJson["ConditionIDs"].is_array()) {
                                    for (const auto& idStr : ruleJson["ConditionIDs"]) rule.conditionIDs.push_back(idStr.get<std::string>());
                                }
                                // 🛡️ 防护：ValuesMapping 必须是对象，否则 .items() 会抛出异常
                                if (ruleJson.contains("ValuesMapping") && ruleJson["ValuesMapping"].is_object()) {
                                    for (auto& el : ruleJson["ValuesMapping"].items()) ParseResultBlock(el.value(), rule.valuesMapping[el.key()]);
                                }
                                if (ruleJson.contains("Result")) ParseResultBlock(ruleJson["Result"], rule.result);
                                ParseMenusSafe(ruleJson, "menus", rule.menus, rule.hasMenus);
                                rule.roundDigits = ParseRoundDigitsSafe(ruleJson, "roundDigits", rule.roundDigits);
                                rule.broadcastOnly = ParseBoolSafe(ruleJson, "broadcastOnly", rule.broadcastOnly);
                                rule.inlineDamage = ParseBoolSafe(ruleJson, "inlineDamage", rule.inlineDamage);
                                if (ruleJson.contains("inlineDamageFormat") && ruleJson["inlineDamageFormat"].is_string()) {
                                    rule.inlineDamageFormat = ruleJson["inlineDamageFormat"].get<std::string>();
                                }
                                rule.inlineDamageDigits = ParseRoundDigitsSafe(ruleJson, "inlineDamageDigits", rule.inlineDamageDigits);
                                g_rules.push_back(rule);
                            }
                        }
                    }
                    catch (...) {}
                }
            }
        }
        catch (const std::filesystem::filesystem_error&) {
            REX::WARN("[IIF] LoadConfigs: directory_iterator failed (dir may have been deleted)");
        }

        REX::INFO("[IIF] LoadConfigs: loaded {} JSON file(s), {} rule(s) in total", fileCount, g_rules.size());

        std::string overridePath = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\CPP_Overrides.json";
        if (std::filesystem::exists(overridePath)) {
            try {
                std::ifstream file(overridePath);
                json j = json::parse(file, nullptr, true, true);
                // 🛡️ 防护：CPP_Overrides 内容必须是对象
                if (j.is_object()) {
                    for (auto& [id, overJson] : j.items()) {
                        CPPOverride over;
                        over.id = id;
                        over.isOverridden = true;
                        if (overJson.contains("priority")) over.userPriority = overJson["priority"].get<int>();
                        if (overJson.contains("anchorTarget")) {
                            if (overJson["anchorTarget"].is_array()) {
                                for (auto& tg : overJson["anchorTarget"]) over.userAnchorTargets.push_back(tg.get<std::string>());
                            }
                            else {
                                over.userAnchorTargets.push_back(overJson["anchorTarget"].get<std::string>());
                            }
                        }
                        if (overJson.contains("anchorMode")) {
                            if (overJson["anchorMode"].is_array()) {
                                for (auto& md : overJson["anchorMode"]) over.userAnchorModes.push_back(md.get<std::string>());
                            }
                            else {
                                over.userAnchorModes.push_back(overJson["anchorMode"].get<std::string>());
                            }
                        }

                        auto it = g_cppOverrides.find(id);
                        if (it != g_cppOverrides.end()) {
                            it->second.userPriority = over.userPriority;
                            it->second.userAnchorTargets = over.userAnchorTargets;
                            it->second.userAnchorModes = over.userAnchorModes;
                            it->second.isOverridden = true;
                        }
                        else {
                            g_cppOverrides[id] = over;
                        }
                    }
                }
            }
            catch (...) {}
        }
    }

    void SaveConfigs() {
        std::unordered_map<std::string, std::vector<JsonRule*>> fileMap;
        for (auto& rule : g_rules) {
            if (!rule.originPath.empty()) fileMap[NormalizeRulePath(rule.originPath)].push_back(&rule);
        }

        const std::string configDir = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\";
        if (std::filesystem::exists(configDir)) {
            try {
                for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
                    if (entry.path().extension() != ".json" && entry.path().extension() != ".jsonc") continue;
                    if (entry.path().filename() == "CPP_Overrides.json") continue;
                    fileMap.emplace(NormalizeRulePath(entry.path().string()), std::vector<JsonRule*>());
                }
            }
            catch (...) {}
        }

        if (fileMap.empty()) return;

        for (auto& [path, rulesInFile] : fileMap) {
            try {
                const std::string actualPath = NormalizeRulePath(path);

                json j;
                try {
                    std::ifstream iFile(actualPath);
                    if (iFile.is_open()) {
                        j = json::parse(iFile, nullptr, true, true);
                        iFile.close();
                    }
                } catch (...) { j = json::object(); }

                if (!rulesInFile.empty() && rulesInFile[0]) {
                    WriteGlobalDefaults(j, *rulesInFile[0]);

                    json rules = json::array();
                    for (auto* rule : rulesInFile) {
                        if (!rule) continue;
                        rules.push_back(SerializeRuleForSave(*rule));
                    }
                    j["DataRules"] = rules;
                } else {
                    j["DataRules"] = json::array();
                }

                std::filesystem::path pPath(actualPath);
                if (!std::filesystem::exists(pPath.parent_path())) {
                    std::filesystem::create_directories(pPath.parent_path());
                }

                std::ofstream oFile(actualPath);
                oFile << std::setw(4) << j << std::endl;
            }
            catch (const std::exception& e) {
                REX::WARN("[IIF] SaveConfigs: error saving {} - {}", path, e.what());
            }
            catch (...) {}
        }

        try {
            std::string overridePath = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\CPP_Overrides.json";
            json jOut;
            for (auto& [id, over] : g_cppOverrides) {
                if (over.isOverridden) {
                    json oJson;
                    oJson["priority"] = over.userPriority;
                    oJson["anchorTarget"] = over.userAnchorTargets;
                    oJson["anchorMode"] = over.userAnchorModes;
                    jOut[id] = oJson;
                }
            }
            std::ofstream oFile(overridePath);
            oFile << std::setw(4) << jOut << std::endl;
        }
        catch (...) {}

        REX::INFO("[IIF] JSON 配置与 C++ 覆写已成功保存。");
    }

    void EvaluateItemRules(RE::TESForm* a_form, RE::BGSInventoryItem* a_item, RE::TBO_InstanceData* a_instance, std::uint32_t a_stackID) {
        if (!a_form) return;

        // 🛡️ SEH 防护：入口处验证 a_instance 指针基本可读性
        // 使用独立的 POD-only lambda 避免 C2712（EvaluateItemRules 函数整体有 std::string 等带析构函数的局部变量）
        // 如果 a_instance 非空但指向已卸载Mod的无效内存，static_cast 后成员访问会崩溃
        // 这里提前验证：一旦读取异常就降级为 nullptr，下游代码自动使用默认数据（weaponData/armorData）
        if (a_instance) {
            auto ValidateInstancePtr = [](RE::TBO_InstanceData*& ptr) {
                __try {
                    volatile auto readable = *reinterpret_cast<volatile const std::uintptr_t*>(ptr);
                    (void)readable;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    ptr = nullptr;
                }
            };
            ValidateInstancePtr(a_instance);
        }

        auto* menuUI = RE::UI::GetSingleton();
        const bool isExamineMenu = menuUI && menuUI->GetMenuOpen("ExamineMenu");

        static auto s_lastCheckTime = std::chrono::steady_clock::now();
        static std::filesystem::file_time_type s_lastModifiedTime;
        static bool s_timeInitialized = false;

        auto now = std::chrono::steady_clock::now();
        if (!s_timeInitialized || std::chrono::duration_cast<std::chrono::seconds>(now - s_lastCheckTime).count() >= 1) {
            s_lastCheckTime = now;
            std::string configDir = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\";
            if (std::filesystem::exists(configDir)) {
                std::filesystem::file_time_type latestTime; bool foundFile = false;
                // 🛡️ TOCTOU 防护：与 LoadConfigs 相同的 directory_iterator 保护
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
                        if (entry.path().extension() == ".json" || entry.path().extension() == ".jsonc") {
                            if (entry.path().filename() == "CPP_Overrides.json") continue;
                            auto ftime = std::filesystem::last_write_time(entry.path());
                            if (!foundFile || ftime > latestTime) { latestTime = ftime; foundFile = true; }
                        }
                    }
                }
                catch (const std::filesystem::filesystem_error&) {
                    REX::WARN("[IIF] EvaluateItemRules: hot-reload scan failed (dir may have been deleted)");
                }
                if (foundFile) {
                    if (!s_timeInitialized) { s_lastModifiedTime = latestTime; s_timeInitialized = true; LoadConfigs(); }
                    else if (latestTime > s_lastModifiedTime) { s_lastModifiedTime = latestTime; LoadConfigs(); }
                }
            }
            else if (!s_timeInitialized) { s_timeInitialized = true; LoadConfigs(); }
        }

        for (const auto& rule : g_rules) {
            if (!IsRuleVisibleInMenu(rule)) continue;

            bool ruleMatched = false; ResultBlock targetResult; float extractedActorValue = -999.0f; RE::TESForm* hitForm = nullptr;

            auto CheckSingleForm = [&](const std::string& idStr) -> bool {
                if (rule.conditionType == "FormType") {
                    if (idStr == "Weapon" || idStr == "WEAP" || idStr == "WeaponType") return a_form->Is(RE::ENUM_FORM_ID::kWEAP);
                    if (idStr == "Armor" || idStr == "ARMO" || idStr == "ArmorType") return a_form->Is(RE::ENUM_FORM_ID::kARMO);
                    if (idStr == "Ammo" || idStr == "AMMO" || idStr == "AmmoType") return a_form->Is(RE::ENUM_FORM_ID::kAMMO);
                    if (idStr == "Alchemy" || idStr == "ALCH" || idStr == "Potion") return a_form->Is(RE::ENUM_FORM_ID::kALCH);
                }

                RE::TESForm* conditionForm = GetFormFromIdentifier(idStr);
                if (!conditionForm) return false; hitForm = conditionForm;

                // 👑 完美还原你原本的 switch-case 逻辑，完全贴合 ENUM_FORM_ID
                switch (conditionForm->formType.get()) {
                case RE::ENUM_FORM_ID::kKYWD: {
                    auto baseKw = a_form->As<RE::BGSKeywordForm>();
                    if (baseKw && baseKw->HasKeyword(conditionForm->As<RE::BGSKeyword>())) return true;
                    if (a_instance) {
                        auto boundObj = a_form->As<RE::TESBoundObject>();
                        if (boundObj && boundObj->Is(RE::ENUM_FORM_ID::kWEAP)) {
                            auto inst = static_cast<RE::TESObjectWEAP::InstanceData*>(a_instance);
                            __try {
                                if (inst->keywords && inst->keywords->HasKeyword(conditionForm->As<RE::BGSKeyword>())) return true;
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                        else if (boundObj && boundObj->Is(RE::ENUM_FORM_ID::kARMO)) {
                            auto inst = static_cast<RE::TESObjectARMO::InstanceData*>(a_instance);
                            __try {
                                if (inst->keywords && inst->keywords->HasKeyword(conditionForm->As<RE::BGSKeyword>())) return true;
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                    return false;
                }
                case RE::ENUM_FORM_ID::kOMOD: {
                    if (a_item) {
                        auto omod = conditionForm->As<RE::BGSMod::Attachment::Mod>();
                        if (omod) {
                            for (auto stack = a_item->stackData.get(); stack; stack = stack->nextStack.get()) {
                                if (stack->extra) {
                                    auto instExtra = stack->extra->GetByType<RE::BGSObjectInstanceExtra>();
                                    // 🛡️ 防御: 确认 instExtra 有效后再调用 HasMod
                                    if (instExtra) {
                                        __try { if (instExtra->HasMod(*omod)) return true; }
                                        __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
                                    }
                                }
                            }
                        }
                    }
                    return false;
                }
                case RE::ENUM_FORM_ID::kAVIF: {
                    float val = GetItemActorValue(a_form, a_instance, conditionForm);
                    if (val > 0.001f || val < -0.001f) { extractedActorValue = val; return true; }
                    return false;
                }
                case RE::ENUM_FORM_ID::kWEAP:
                case RE::ENUM_FORM_ID::kARMO:
                    return a_form->GetFormID() == conditionForm->GetFormID();
                default:
                    return false;
                }
                };

            if (!rule.valuesMapping.empty()) {
                for (auto& mapPair : rule.valuesMapping) {
                    if (CheckSingleForm(mapPair.first)) { ruleMatched = true; targetResult = mapPair.second; break; }
                }
            }
            else if (!rule.conditionIDs.empty()) {
                bool allMatched = true; bool anyMatched = false;
                for (auto& idStr : rule.conditionIDs) {
                    bool match = CheckSingleForm(idStr);
                    if (match) anyMatched = true; else allMatched = false;
                }
                if (rule.matchType == "AND" && allMatched) ruleMatched = true;
                else if (rule.matchType != "AND" && anyMatched) ruleMatched = true;
                if (ruleMatched) targetResult = rule.result;
            }

            if (ruleMatched) {
                // 弹丸数并入原版伤害行：只在规则数据源确实是 NumProjectiles 时生效，
                // 并且弹丸数 > 1 才需要把伤害拆成「单发 x 数量」。
                // broadcastOnly 规则不产生任何可见改动，所以 inline 改写也让位给广播条目。
                if (rule.inlineDamage && !rule.broadcastOnly && RuleUsesNumProjectiles(rule, targetResult)) {
                    float projectileCount = 0.0f;
                    if (!Hooks::g_inlineDamage.active && ReadWeaponNumProjectiles(a_form, a_instance, projectileCount) && projectileCount > 1.0f) {
                        Hooks::g_inlineDamage.active = true;
                        Hooks::g_inlineDamage.projectiles = projectileCount;
                        Hooks::g_inlineDamage.format = rule.inlineDamageFormat.empty() ? std::string("{damage} x {count}") : rule.inlineDamageFormat;
                        Hooks::g_inlineDamage.roundDigits = rule.inlineDamageDigits;
                    }
                    // inline 模式下不再单独生成一张卡
                    continue;
                }

                Hooks::InternalCard ic{};
                ic.text = rule.id; ic.label = rule.titleText; ic.highlightLabel = rule.highlightLabel;
                ic.priority = rule.priority; ic.displayType = rule.displayType;
                ic.hasBackground = rule.hasBackground; ic.backgroundColor = rule.backgroundColor;
                ic.anchorTargets = rule.anchorTargets; ic.anchorModes = rule.anchorModes;
                ic.sortValueFrom = rule.sortValueFrom;
                ic.broadcastOnly = rule.broadcastOnly;

                if (targetResult.hasContent && !targetResult.leftBox.active && !targetResult.rightBox.active) {
                    targetResult.rightBox.active = true; targetResult.rightBox.value = targetResult.value;
                    if (!targetResult.tag.empty()) targetResult.rightBox.tag = targetResult.tag;
                    if (!targetResult.state.empty()) targetResult.rightBox.state = targetResult.state;
                    if (!targetResult.align.empty()) targetResult.rightBox.align = targetResult.align;
                    targetResult.rightBox.isIcon = targetResult.isIcon;
                    if (targetResult.dataSource.active) targetResult.rightBox.dataSource = targetResult.dataSource;
                }

                auto ExtractFloatValue = [&](const DataSource& ds, RE::TESForm* form, const RE::BGSInventoryItem* item, RE::TBO_InstanceData* instance, std::uint32_t stackID, bool& success) -> float {
                    success = false;
                    if (!ds.active) return 0.0f;
                    // 🛡️ 防护：form 可能为 null（来自已卸载Mod的已装备物品对比）
                    if (!form) return 0.0f;

                    if (ds.Type == "WeaponStat") {
                        if (form->Is(RE::ENUM_FORM_ID::kWEAP)) {
                            auto weap = form->As<RE::TESObjectWEAP>();
                            // 🛡️ 防护：As<> 失败时直接跳过（来自Mod的非标准kWEAP form）
                            if (!weap) return 0.0f;
                            auto weapData = instance ? static_cast<RE::TESObjectWEAP::InstanceData*>(instance) : &weap->weaponData;
                            if (weapData) {
                                // Some non-ranged weapon instances leave ranged-only/scalar fields
                                // uninitialized. Do not expose those bit patterns to Scaleform.
                                auto FinishWeaponStat = [&](float value) -> float {
                                    if (!std::isfinite(value) || std::abs(value) > 100000.0f) {
                                        success = false;
                                        return 0.0f;
                                    }
                                    success = true;
                                    return value;
                                };

                                // 🛡️ SEH防护：weapData 的 static_cast 可能指向类型不匹配的内存
                                __try {
                                    if (ds.ID == "APCost" || ds.ID == "AttackActionPointCost") {
                                        float val = weapData->attackActionPointCost;
                                        return FinishWeaponStat(ApplyPerkModifier(static_cast<std::uint32_t>(RE::BGSEntryPoint::ENTRY_POINT::kModVATSAttackAP), val, weap));
                                    }
                                    if (ds.ID == "Speed") {
                                        return FinishWeaponStat(weapData->speed);
                                    }
                                    if (ds.ID == "Reach") {
                                        return FinishWeaponStat(weapData->reach);
                                    }
                                    if (ds.ID == "SecondaryDamage") {
                                        return FinishWeaponStat(weapData->secondaryDamage);
                                    }
                                    if (ds.ID == "AmmoCapacity") {
                                        return FinishWeaponStat(static_cast<float>(weapData->ammoCapacity));
                                    }
                                    // 一次发射的弹丸总数（霰弹枪/分裂枪管等）。实例数据已包含 OMOD 修改。
                                    if (ds.ID == "NumProjectiles") {
                                        float count = 0.0f;
                                        if (!ReadWeaponNumProjectiles(form, instance, count)) return 0.0f;
                                        return FinishWeaponStat(count);
                                    }
                                    if (ds.ID == "ReloadSpeed") {
                                        return FinishWeaponStat(weapData->reloadSpeed);
                                    }
                                    if (ds.ID == "SightedTransitionSeconds") {
                                        if (!weapData->rangedData) return 0.0f;
                                        return FinishWeaponStat(weapData->rangedData->sightedTransitionSeconds);
                                    }
                                    if (ds.ID == "AimModelRecoilMaxDegPerShot") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelRecoilMaxDegPerShot);
                                    }
                                    if (ds.ID == "AimModelRecoilMinDegPerShot") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelRecoilMinDegPerShot);
                                    }
                                    if (ds.ID == "AimModelRecoilArcDeg") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelRecoilArcDeg);
                                    }
                                    if (ds.ID == "AimModelRecoilArcRotateDeg") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelRecoilArcRotateDeg);
                                    }
                                    if (ds.ID == "AimModelMaxConeDegrees") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelMaxConeDegrees);
                                    }
                                    if (ds.ID == "AimModelMinConeDegrees") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelMinConeDegrees);
                                    }
                                    if (ds.ID == "AimModelConeIronSightsMultiplier") {
                                        if (!weapData->aimModel) return 0.0f;
                                        return FinishWeaponStat(weapData->aimModel->aimModelData.aimModelConeIronSightsMultiplier);
                                    }
                                    if (ds.ID == "ZoomDataFOVMult") {
                                        if (!weapData->zoomData) return 0.0f;
                                        return FinishWeaponStat(weapData->zoomData->zoomData.fovMult);
                                    }
                                    if (ds.ID == "CritCharge") {
                                        float val = weapData->criticalChargeBonus;
                                        return FinishWeaponStat(ApplyPerkModifier(static_cast<std::uint32_t>(RE::BGSEntryPoint::ENTRY_POINT::kModVATSCriticalCharge), val, weap));
                                    }
                                    if (ds.ID == "CritDamage") {
                                        float val = weapData->criticalDamageMult;
                                        return FinishWeaponStat(ApplyPerkModifier(static_cast<std::uint32_t>(RE::BGSEntryPoint::ENTRY_POINT::kCalculateCriticalHitDamageMult), val, weap));
                                    }
                                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                            }
                        }
                    }
                    else if (ds.Type == "VATS_Accuracy") {
                        float totalBonus = 0.0f;
                        success = true;
                        RE::TESObjectWEAP* weapForPerk = (form && form->Is(RE::ENUM_FORM_ID::kWEAP)) ? form->As<RE::TESObjectWEAP>() : nullptr;
                        if (item && item->stackData) {
                            const RE::BGSInventoryItem::Stack* targetStack = nullptr;
                            if (stackID == static_cast<std::uint32_t>(-1) || stackID == 0) {
                                for (auto s = item->stackData.get(); s; s = s->nextStack.get()) {
                                    if (s->extra && s->extra->GetByType<RE::BGSObjectInstanceExtra>()) {
                                        targetStack = s; break;
                                    }
                                }
                            }
                            else { targetStack = item->GetStackByID(stackID); }

                            if (targetStack && targetStack->extra) {
                                auto instExtra = targetStack->extra->GetByType<RE::BGSObjectInstanceExtra>();
                                if (instExtra) {
                                    // 🛡️ SEH防护（外层）：GetIndexData() 和 range-for 的迭代器解引用可能访问损坏数据
                                    // 改用索引循环将整个数据访问置于 __try 保护之下（对付投掷物无 OMOD 数据的场景）
                                    __try {
                                        const auto& idxDataRef = instExtra->GetIndexData();
                                        std::uint32_t idxTotal = static_cast<std::uint32_t>(idxDataRef.size());
                                        for (std::uint32_t idx = 0; idx < idxTotal; ++idx) {
                                            auto& modData = idxDataRef[idx];
                                            RE::TESForm* rawForm = RE::TESForm::GetFormByID(modData.objectID);
                                            auto omod = rawForm ? rawForm->As<RE::BGSMod::Attachment::Mod>() : nullptr;
                                            if (!omod) continue;

                                            // 🛡️ SEH防护（内层）：Container::GetData() 和 propertyMods 迭代可能访问到损坏的游戏数据
                                            __try {
                                                RE::BGSMod::Container::Data omodData{};
                                                static_cast<const RE::BGSMod::Container*>(omod)->GetData(&omodData);
                                                // 🛡️ 防御: propertyMods 为空但计数>0时跳过，或计数明显异常时跳过
                                                if (!omodData.propertyMods || omodData.propertyModCount > 512 || omodData.propertyModCount == 0) continue;
                                                for (std::uint32_t i = 0; i < omodData.propertyModCount; ++i) {
                                                    auto& prop = omodData.propertyMods[i];
                                                    std::uint32_t targetType = static_cast<std::uint32_t>(prop.target);

                                                    if (targetType == 76 && prop.type == RE::BGSMod::Property::TYPE::kFloat) {
                                                        totalBonus += prop.data.mm.min.f;
                                                    }
                                                    else if ((targetType == 77 || targetType == 43) && prop.type == RE::BGSMod::Property::TYPE::kPair) {
                                                        if (prop.data.fv.formID == 0x7B) {
                                                            totalBonus += prop.data.fv.value;
                                                        }
                                                    }
                                                }
                                            } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
                                        }
                                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                                }
                            }
                        }
                        // 🛡️ 把 perk 对 VATS 命中率的修正也叠加进去（以 0 为基准，得到 perk 加成增量）
                        // 注意：仅对 add 操作的 perk 修正生效；mul 类操作对 0 输入会被吞掉，无法显示
                        if (weapForPerk) {
                            totalBonus += ApplyPerkModifier(static_cast<std::uint32_t>(RE::BGSEntryPoint::ENTRY_POINT::kModVATSHitChance), 0.0f, weapForPerk);
                        }
                        return totalBonus;
                    }
                    else if (ds.Type == "OMODMagicMagnitude") {
                        float totalBonus = 0.0f;
                        success = true;
                        bool isAny = (ds.ID == "ANY");
                        RE::TESForm* targetOmodForm = isAny ? nullptr : GetFormFromIdentifier(ds.ID);

                        if (item && item->stackData) {
                            const RE::BGSInventoryItem::Stack* targetStack = nullptr;
                            if (stackID == static_cast<std::uint32_t>(-1) || stackID == 0) {
                                for (auto s = item->stackData.get(); s; s = s->nextStack.get()) {
                                    if (s->extra && s->extra->GetByType<RE::BGSObjectInstanceExtra>()) {
                                        targetStack = s; break;
                                    }
                                }
                            }
                            else { targetStack = item->GetStackByID(stackID); }

                            if (targetStack && targetStack->extra) {
                                auto instExtra = targetStack->extra->GetByType<RE::BGSObjectInstanceExtra>();
                                if (instExtra) {
                                    // 🛡️ SEH防护（外层）：GetIndexData() 和 range-for 的迭代器解引用可能访问损坏数据
                                    __try {
                                        const auto& idxDataRef = instExtra->GetIndexData();
                                        std::uint32_t idxTotal = static_cast<std::uint32_t>(idxDataRef.size());
                                        for (std::uint32_t idx = 0; idx < idxTotal; ++idx) {
                                            auto& modData = idxDataRef[idx];
                                            RE::TESForm* rawForm = RE::TESForm::GetFormByID(modData.objectID);
                                            auto omod = rawForm ? rawForm->As<RE::BGSMod::Attachment::Mod>() : nullptr;
                                            if (!omod) continue;

                                            if (!isAny && targetOmodForm && omod->GetFormID() != targetOmodForm->GetFormID()) continue;

                                            // 🛡️ SEH防护（内层）：Container::GetData() 和 propertyMods 迭代
                                            __try {
                                                RE::BGSMod::Container::Data omodData{};
                                                static_cast<const RE::BGSMod::Container*>(omod)->GetData(&omodData);
                                                // 🛡️ 防御: propertyMods 为空但计数>0时跳过，或计数明显异常时跳过
                                                if (!omodData.propertyMods || omodData.propertyModCount > 512 || omodData.propertyModCount == 0) continue;
                                                for (std::uint32_t i = 0; i < omodData.propertyModCount; ++i) {
                                                    auto& prop = omodData.propertyMods[i];
                                                    if (prop.type == RE::BGSMod::Property::TYPE::kFloat) {
                                                        totalBonus += prop.data.mm.min.f;
                                                    }
                                                    else if (prop.type == RE::BGSMod::Property::TYPE::kPair) {
                                                        totalBonus += prop.data.fv.value;
                                                    }
                                                }
                                            } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
                                        }
                                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                                }
                            }
                        }
                        return totalBonus;
                    }
                    else if (ds.Type == "PerkRank") {
                        RE::TESForm* perkForm = GetFormFromIdentifier(ds.ID);
                        if (perkForm && perkForm->Is(RE::ENUM_FORM_ID::kPERK)) {
                            success = true;
                            float rankVal = 0.0f;
                            RE::TESForm* playerBase = RE::TESForm::GetFormByID(0x00000007);
                            if (playerBase) {
                                auto perkArray = playerBase->As<RE::BGSPerkRankArray>();
                                // 🛡️ SEH防护 + 边界检查：perkCount 异常过大时跳过
                    __try {
                        if (perkArray && perkArray->perks && perkArray->perkCount < 1024) {
                            for (std::uint32_t i = 0; i < perkArray->perkCount; ++i) {
                                if (perkArray->perks[i].perk == perkForm) {
                                    rankVal = static_cast<float>(perkArray->perks[i].currentRank);
                                    if (rankVal == 0.0f) rankVal = 1.0f;
                                    break;
                                }
                            }
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                            }
                            return rankVal;
                        }
                    }
                    else {
                        RE::TESForm* dsForm = GetFormFromIdentifier(ds.ID);
                        if (dsForm && dsForm->Is(RE::ENUM_FORM_ID::kAVIF)) {
                            success = true; return GetItemActorValue(form, instance, dsForm);
                        }
                    }
                    return 0.0f;
                    };

                auto GetNonOGWeaponDiff = [&](const DataSource& ds, float previewVal, float multiplier, float& outDiff, bool& outHasDiff) {
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (!player) return;

                    for (std::uint32_t equipIndex = 0; equipIndex < 2; ++equipIndex) {
                        RE::BGSObjectInstance equipped(nullptr, nullptr);
                        auto* result = player->GetEquippedItem(&equipped, RE::BGSEquipIndex{ equipIndex });
                        if (!result || !equipped.object || !equipped.object->Is(RE::ENUM_FORM_ID::kWEAP)) continue;

                        bool eSuccess = false;
                        float eqVal = ExtractFloatValue(ds, equipped.object, nullptr, equipped.instanceData.get(), 0, eSuccess);
                        if (eSuccess) {
                            outDiff = (previewVal - eqVal) * multiplier;
                            outHasDiff = true;
                        }
                        return;
                    }
                };

                // 🛡️ 独立 lambda：安全获取已装备物品的对比差值（所有局部变量均为POD，无析构函数，避免C2712）
                auto SafeGetEquippedDiff = [&](RE::TESBoundObject* boundObj, const DataSource& ds, float previewVal, float multiplier, float& outDiff, bool& outHasDiff) {
                    // CommonLibF4 marks UIUtils::GetComparisonItems as inlined on NG/AE.
                    // Use the version-stable Actor API for weapon stats on those runtimes.
                    if (!REX::FModule::IsRuntimeOG()) {
                        if (!boundObj || !boundObj->Is(RE::ENUM_FORM_ID::kWEAP) || ds.Type != "WeaponStat") return;
                        GetNonOGWeaponDiff(ds, previewVal, multiplier, outDiff, outHasDiff);
                        return;
                    }

                    auto* compItems = new RE::UIUtils::ComparisonItems();
                    bool compOk = false;
                    __try { RE::UIUtils::GetComparisonItems(boundObj, *compItems); compOk = true; }
                    __except (EXCEPTION_EXECUTE_HANDLER) { delete compItems; compItems = nullptr; }
                    if (compOk && compItems && !compItems->empty()) {
                        __try {
                            const void* tuplePtr = &compItems->front();
                            const RE::BGSInventoryItem* eqItem = *reinterpret_cast<const RE::BGSInventoryItem* const*>(tuplePtr);
                            std::uint32_t eqStackID = *reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const uint8_t*>(tuplePtr) + sizeof(void*));
                            if (eqItem && eqItem->object) {
                                auto eqInst = eqItem->GetInstanceData(eqStackID);
                                bool eSuccess = false;
                                float eqVal = ExtractFloatValue(ds, eqItem->object, eqItem, eqInst, eqStackID, eSuccess);
                                if (eSuccess) {
                                    outDiff = (previewVal - eqVal) * multiplier;
                                    outHasDiff = true;
                                }
                            }
                        } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                    delete compItems;
                };

                auto ResolveBox = [&](const BoxConfig& globalBox, const BoxConfig& localBox, std::string& outTag, bool& outIsIcon, std::string& outVal, bool& outBad, bool& outGood, bool& outStandard, std::string& outAlign, bool& outHasRaw, double& outRaw, float& outDifference, bool& outHasDiff) -> bool {
                    BoxConfig finalBox = globalBox;
                    if (localBox.active) {
                        if (!localBox.tag.empty()) finalBox.tag = localBox.tag;
                        finalBox.isIcon = localBox.isIcon;
                        if (!localBox.value.empty()) finalBox.value = localBox.value;
                        if (!localBox.state.empty()) finalBox.state = localBox.state;
                        if (!localBox.align.empty()) finalBox.align = localBox.align;
                        if (localBox.dataSource.active) finalBox.dataSource = localBox.dataSource;
                    }
                    outHasRaw = false; outRaw = 0.0; outDifference = 0.0f; outHasDiff = false;

                    if (finalBox.dataSource.active) {
                        bool pSuccess = false;
                        float previewVal = ExtractFloatValue(finalBox.dataSource, a_form, a_item, a_instance, a_stackID, pSuccess);

                        if (!pSuccess) {
                            // A failed pointer-backed lookup must not create a card with
                            // an empty GFx value, which Scaleform renders as "undefined".
                            return false;
                        }

                        if (pSuccess) {
                            float finalPreviewVal = (previewVal + finalBox.dataSource.Offset) * finalBox.dataSource.Multiplier;
                            const int roundDigits = finalBox.dataSource.roundDigits >= 0 ? finalBox.dataSource.roundDigits : rule.roundDigits;

                            outHasRaw = true; outRaw = RoundToDigits(finalPreviewVal, roundDigits);

                            std::string prefix = ((finalBox.dataSource.Type == "VATS_Accuracy" || finalBox.dataSource.Type == "OMODMagicMagnitude") && finalPreviewVal > 0.001f) ? "+" : "";
                            finalBox.value = prefix + FormatValue(finalPreviewVal, roundDigits) + finalBox.dataSource.Suffix;

                            bool isEquipped = false;
                            if (a_item && a_item->stackData) {
                                if (a_stackID == 0 || a_stackID == static_cast<std::uint32_t>(-1)) {
                                    for (auto s = a_item->stackData.get(); s; s = s->nextStack.get()) {
                                        if (s->flags.any(RE::BGSInventoryItem::Stack::Flag::kSlotMask)) {
                                            isEquipped = true; break;
                                        }
                                    }
                                }
                                else {
                                    auto s = a_item->GetStackByID(a_stackID);
                                    if (s && s->flags.any(RE::BGSInventoryItem::Stack::Flag::kSlotMask)) {
                                        isEquipped = true;
                                    }
                                }
                            }

                            if (!isExamineMenu && !isEquipped && a_form && a_form->IsBoundObject()) {
                                auto boundObj = a_form->As<RE::TESBoundObject>();
                                if (boundObj) {
                                    SafeGetEquippedDiff(boundObj, finalBox.dataSource, previewVal, finalBox.dataSource.Multiplier, outDifference, outHasDiff);
                                }
                            }

                            if (outHasDiff) {
                                // Match the value's display precision so the diff never shows
                                // digits the value itself is hiding.
                                const int diffDigits = roundDigits >= 0 ? roundDigits : 1;
                                outDifference = static_cast<float>(RoundToDigits(outDifference, diffDigits));
                                if (outDifference == 0.0f) {
                                    outDifference = 0.0f;
                                    outHasDiff = false;
                                }
                            }
                        }
                    }
                    else if (finalBox.value.empty() && extractedActorValue != -999.0f) {
                        finalBox.value = FormatValue(extractedActorValue);
                    }

                    std::string currentTag = finalBox.tag; std::string currentVal = finalBox.value;
                    if (hitForm) {
                        auto nameObj = hitForm->As<RE::TESFullName>();
                        if (nameObj && nameObj->GetFullName()) {
                            std::string gameName = nameObj->GetFullName();
                            if (currentTag == "@GameName") currentTag = gameName;
                            if (currentVal == "@GameName") currentVal = gameName;
                        }
                    }
                    outIsIcon = !finalBox.isIcon;
                    if (finalBox.isIcon && !currentTag.empty()) { outTag = (currentTag.front() == '[' && currentTag.back() == ']') ? currentTag : "[" + currentTag + "]"; }
                    else { outTag = currentTag; }

                    outVal = currentVal;
                    outBad = (finalBox.state == "bad"); outGood = (finalBox.state == "good"); outStandard = (finalBox.state == "standard");
                    outAlign = finalBox.align.empty() ? "right" : finalBox.align;
                    return true;
                    };

                bool leftHasRaw = false; double leftRaw = 0; float leftDiff = 0.0f; bool leftHasDiff = false;
                bool rightHasRaw = false; double rightRaw = 0; float rightDiff = 0.0f; bool rightHasDiff = false;

                bool leftResolved = ResolveBox(rule.globalLeftBox, targetResult.leftBox, ic.icon1, ic.icon1IsText, ic.val1, ic.val1Bad, ic.val1Good, ic.val1Standard, ic.align1, leftHasRaw, leftRaw, leftDiff, leftHasDiff);
                bool rightResolved = ResolveBox(rule.globalRightBox, targetResult.rightBox, ic.icon2, ic.icon2IsText, ic.val2, ic.val2Bad, ic.val2Good, ic.val2Standard, ic.align2, rightHasRaw, rightRaw, rightDiff, rightHasDiff);
                if (!leftResolved || !rightResolved) continue;

                ic.hasRawValue = rightHasRaw ? rightHasRaw : leftHasRaw;
                ic.rawValue = rightHasRaw ? rightRaw : leftRaw;

                ic.hasDifference = rightHasDiff || leftHasDiff;
                ic.difference = rightHasDiff ? rightDiff : leftDiff;

                ic.suffix = targetResult.rightBox.active ? targetResult.rightBox.dataSource.Suffix : (targetResult.leftBox.active ? targetResult.leftBox.dataSource.Suffix : "");

                bool originalHide = targetResult.hideDifference != -1 ? (targetResult.hideDifference == 1) : (rule.hideDifference == 1);
                ic.showDifference = !originalHide && ic.hasDifference;

                ic.invertDiffColor = targetResult.invertDiffColor != -1 ? (targetResult.invertDiffColor == 1) : (rule.invertDiffColor == 1);
                ic.fillPct = targetResult.fillPct != -1.0f ? targetResult.fillPct : rule.fillPct;
                ic.shieldPct = targetResult.shieldPct != 0.0f ? targetResult.shieldPct : rule.shieldPct;
                ic.fillColor = targetResult.fillColor != 0 ? targetResult.fillColor : rule.fillColor;
                ic.showBar = targetResult.showBar != -1 ? (targetResult.showBar == 1) : rule.showBar;
                ic.showValue = targetResult.showValue != -1 ? (targetResult.showValue == 1) : rule.showValue;
                ic.valueAlign = !targetResult.valueAlign.empty() ? targetResult.valueAlign : rule.valueAlign;
                ic.valueColor = targetResult.valueColor != 0 ? targetResult.valueColor : rule.valueColor;
                ic.valueStandard = targetResult.valueStandard != -1 ? (targetResult.valueStandard == 1) : rule.valueStandard;
                ic.valueBad = (targetResult.state == "bad" || rule.state == "bad");
                ic.valueGood = (targetResult.state == "good" || rule.state == "good");
                if (targetResult.state == "standard" || rule.state == "standard") ic.valueStandard = true;

                std::string vText = !targetResult.valueText.empty() ? targetResult.valueText : rule.valueText;
                if (hitForm && vText == "@GameName") {
                    auto nameObj = hitForm->As<RE::TESFullName>();
                    if (nameObj && nameObj->GetFullName()) vText = nameObj->GetFullName();
                }
                ic.valueText = vText;

                if (ic.displayType == 0 || ic.displayType == 1) {
                    if (ic.value.empty() && !ic.val2.empty()) ic.value = ic.val2;
                    if (ic.valueText.empty() && !ic.val2.empty()) ic.valueText = ic.val2;
                    if (ic.val2Standard) ic.valueStandard = true;
                    if (ic.val2Bad) ic.valueBad = true;
                    if (ic.val2Good) ic.valueGood = true;
                }
                Hooks::g_pendingCards.push_back(ic);
            }
        }
    }
}
