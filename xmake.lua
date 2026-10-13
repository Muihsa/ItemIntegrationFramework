-- ItemIntegrationFramework xmake project
local plugin_name = "ItemIntegrationFramework"
local plugin_version = "1.0.5"
includes(path.join(os.scriptdir(), "..", "..", "shared", "xmake", "commonlibf4.lua"))
local commonlibf4_path = resolve_commonlib4(os.scriptdir())
if not commonlibf4_path then
    raise("The project CommonLibF4 profile could not be resolved")
end
includes(commonlibf4_path)
set_languages("c++23")
add_requires("simpleini v4.25")
add_requires("nlohmann_json v3.12.0")
add_requires("tinyxml2 11.0.0")
add_requires("imgui v1.92.5-docking", { configs = { win32 = true, dx11 = true } })
add_requires("microsoft-detours 2023.6.8")
add_requires("minhook v1.3.4")
local plugin_version_major, plugin_version_minor, plugin_version_patch = plugin_version:match("^(%d+)%.(%d+)%.(%d+)$")

target(plugin_name)
    set_group("Projects")
    add_rules("commonlibf4.plugin", {
        name = plugin_name,
        author = "h_wushen",
        description = "Shared item card injection and hook framework for Fallout 4",
        version = plugin_version
    })

    add_deps("commonlibf4")
    add_packages("simpleini", "nlohmann_json", "tinyxml2", "imgui", "microsoft-detours", "minhook")
    on_load(function (target)
        local imgui = target:pkg("imgui")
        if imgui then
            local include_root = path.join(imgui:installdir(), "include")
            target:add("includedirs", include_root)
            target:add("includedirs", path.join(include_root, "backends"))
        end
        local detours = target:pkg("microsoft-detours")
        if detours then
            target:add("includedirs", path.join(detours:installdir(), "include"))
        end
        local include_packages = { minhook = true, nlohmann_json = true }
        for _, package in ipairs(target:orderpkgs()) do
            if include_packages[package:name()] then
                target:add("includedirs", path.join(package:installdir(), "include"))
            end
        end
    end)

    add_files("src/**.cpp")
    remove_files("src/GUIEditor/**.cpp")
    add_headerfiles("src/**.h")
    remove_files("src/GUIEditor/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

    add_defines(
        'PLUGIN_NAME="' .. plugin_name .. '"',
        "PLUGIN_VERSION_MAJOR=" .. plugin_version_major,
        "PLUGIN_VERSION_MINOR=" .. plugin_version_minor,
        "PLUGIN_VERSION_PATCH=" .. plugin_version_patch
    )

    set_encodings("utf-8")
    add_cxxflags("/utf-8", { force = true, tools = { "msvc", "clang-cl" } })

    add_installfiles("data/F4SE/Plugins/IIF_Type0_Doc.json", { prefixdir = "F4SE/Plugins" })
    add_installfiles("data/F4SE/Plugins/IIF_Type1_Doc.json", { prefixdir = "F4SE/Plugins" })
    add_installfiles("data/F4SE/Plugins/IIF_Type2_Doc.json", { prefixdir = "F4SE/Plugins" })
    add_installfiles("data/F4SE/Plugins/IIF_Type0_文档.json", { prefixdir = "F4SE/Plugins" })
    add_installfiles("data/F4SE/Plugins/IIF_Type1_文档.json", { prefixdir = "F4SE/Plugins" })
    add_installfiles("data/F4SE/Plugins/IIF_Type2_文档.json", { prefixdir = "F4SE/Plugins" })
    add_installfiles("data/F4SE/Plugins/ItemIntegrationFramework/*.json", { prefixdir = "F4SE/Plugins/ItemIntegrationFramework" })
    add_installfiles("data/F4SE/Plugins/ItemIntegrationFramework/README.txt", { prefixdir = "F4SE/Plugins/ItemIntegrationFramework" })
    add_installfiles("data/F4SE/Plugins/ItemIntegrationFramework/font.ttf", { prefixdir = "F4SE/Plugins/ItemIntegrationFramework" })
    add_installfiles("data/F4SE/Plugins/ItemIntegrationFramework/lang/*.json", { prefixdir = "F4SE/Plugins/ItemIntegrationFramework/lang" })

    on_config(function (target)
        if os.getenv("XSE_FO4_GAME_PATH") then
            target:set("installdir", path.join(os.getenv("XSE_FO4_GAME_PATH"), "Data"))
            target:add("installfiles", target:targetfile(), { prefixdir = "F4SE/Plugins" })
            target:add("installfiles", target:symbolfile(), { prefixdir = "F4SE/Plugins" })
        end
    end)
