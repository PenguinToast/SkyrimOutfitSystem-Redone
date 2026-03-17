-- set minimum xmake version
set_xmakever("3.0.0")

includes("lib/commonlibsse-ng")

set_project("SkyrimOutfitSystemNG")
set_version("0.1.0")
set_license("GPL-3.0")

set_languages("c++23")
set_warnings("allextra")

set_policy("package.requires_lock", true)

add_rules("mode.release", "mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("SkyrimOutfitSystemNG")
    add_deps("commonlibsse-ng")

    add_rules("commonlibsse-ng.plugin", {
        name = "Skyrim Outfit System NG",
        author = "PenguinToast",
        description = "SKSE64 plugin using CommonLibSSE-NG and PrismaUI"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
