set_xmakever("3.0.0")

if is_plat("windows") then
    local commonlib_path = os.getenv("COMMONLIBF4_PATH") or "../../commonlibf4-nomad"
    if not os.isdir(path.join(commonlib_path, "lib", "commonlib-shared", "include")) then
        raise("CommonLib check FAILED: '" .. commonlib_path .. "' is not an initialised commonlibf4-nomad checkout. " ..
            "Set COMMONLIBF4_PATH to a G-A-R-D-E-N/commonlibf4-nomad clone with submodules.")
    end
    includes(commonlib_path)
end

set_project("FalloutAnomalyRadiation")
set_version("1.0.0")
set_languages("c++23")

set_warnings("allextra")
set_encodings("utf-8")

if is_plat("windows") then
    add_requires("spdlog v1.16.0", "fmt", "nlohmann_json")
end

add_rules("mode.release")

target("FalloutAnomalyRadiation")
    set_kind("shared")
    set_arch("x64")
    set_filename("FalloutAnomalyRadiation.dll")

    if is_plat("windows") then
        add_deps("commonlibf4")
    end

    if is_plat("windows") then
        add_packages("spdlog", "fmt", "nlohmann_json")
    end

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")

    set_pcxxheader("src/pch.h")

    if is_plat("windows") then
        add_syslinks("bcrypt", "version", "User32", "Ole32", "OleAut32", "crypt32", "Shell32")
    end

    set_targetdir("build/release")

    after_build(function (target)
        if os.getenv("GITHUB_ACTIONS") then
            return
        end
        local dll = target:targetfile()
        local dests = {}
        local mods = os.getenv("XSE_FO4_MODS_PATH")
        if mods then
            table.insert(dests, path.join(mods, "RadiationOverhaul", "F4SE", "Plugins"))
        end
        table.insert(dests, "C:/Modlists/TMR/mods/Fallout Anomaly - Radiation Overhaul/F4SE/Plugins")
        table.insert(dests, "E:/Modlists/Fallen World Alpha 2/mods/RadiationOverhaul/F4SE/Plugins")
        for _, d in ipairs(dests) do
            if os.isdir(d) then
                if os.trycp(dll, path.join(d, "FalloutAnomalyRadiation.dll")) then
                    print("[deploy] FalloutAnomalyRadiation.dll -> " .. d)
                else
                    print("[deploy] SKIPPED (locked? close FO4) -> " .. d)
                end
            end
        end
    end)

target("rad_correctness_policy_tests")
    set_kind("binary")
    add_files("tests/correctness_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("climate_policy_tests")
    set_kind("binary")
    add_files("tests/climate_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("climate_weather_policy_tests")
    set_kind("binary")
    add_files("tests/climate_weather_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("climate_wetness_policy_tests")
    set_kind("binary")
    add_files("tests/climate_wetness_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("needs_policy_tests")
    set_kind("binary")
    add_files("tests/needs_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("needs_tuning_policy_tests")
    set_kind("binary")
    add_files("tests/needs_tuning_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("needs_trace_policy_tests")
    set_kind("binary")
    add_files("tests/needs_trace_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")

target("serialization_policy_tests")
    set_kind("binary")
    add_files("tests/serialization_policy_tests.cpp")
    add_includedirs("src")
    set_targetdir("build/tests")
