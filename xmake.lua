-- set minimum xmake version
set_xmakever("3.0.0")

includes("lib/commonlibsse-ng")

set_project("SkyrimOutfitSystemRedone")
set_version("0.1.0")
set_license("GPL-3.0")

set_languages("c++23")
set_warnings("allextra")

set_policy("package.requires_lock", true)

add_rules("mode.release", "mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("nlohmann_json v3.12.0")

target("SkyrimOutfitSystemRedone")
    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")

    add_rules("commonlibsse-ng.plugin", {
        name = "Skyrim Outfit System Redone",
        author = "PenguinToast",
        description = "SKSE64 plugin using CommonLibSSE-NG and Dear ImGui"
    })

    add_files("src/**.cpp")
    add_files(
        "lib/imgui/imgui.cpp",
        "lib/imgui/imgui_draw.cpp",
        "lib/imgui/imgui_tables.cpp",
        "lib/imgui/imgui_widgets.cpp",
        "lib/imgui/backends/imgui_impl_dx11.cpp",
        "lib/imgui/backends/imgui_impl_win32.cpp"
    )
    add_headerfiles("src/**.h")
    add_includedirs("src", "lib/imgui", "lib/imgui/backends")
    add_syslinks("d3d11", "dxgi")
    set_pcxxheader("src/pch.h")
