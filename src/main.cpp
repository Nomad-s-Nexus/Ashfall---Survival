#include "pch.h"
#include "RadiationEngine.h"
#include "ClimateRuntime.h"
#include "NeedsRuntime.h"
#include "OutbreakQuestHeld.h"
#include "OutbreakRuntime.h"
#include "Hooks.h"
#include "RadHud.h"
#include "RadCorrectnessPolicy.h"
#include "RadLifecycle.h"
#include "RadLifecycleDispatch.h"
#include "RadNeedsTuning.h"
#include "RadReliefConfig.h"
#include "ClimateEffects.h"
#include <RE/U/UI.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/C/ControlMap.h>
#include <thread>
#include <atomic>
#include <ShlObj.h>
#include <KnownFolders.h>

namespace
{
    std::shared_ptr<spdlog::logger> logger;

    enum class GameVersion
    {
        Unknown,
        OG,
        NewGen,
        AE
    };

    GameVersion g_gameVersion = GameVersion::Unknown;

    GameVersion DetectGameVersion(const F4SE::QueryInterface* a_f4se)
    {
        const auto ver = a_f4se->RuntimeVersion();

        if (ver == F4SE::RUNTIME_1_10_163)
        {
            return GameVersion::OG;
        }

        if (ver >= F4SE::RUNTIME_1_10_980 && ver <= F4SE::RUNTIME_1_10_984)
        {
            return GameVersion::NewGen;
        }

        if (ver == F4SE::RUNTIME_1_11_240)
        {
            return GameVersion::AE;
        }

        return GameVersion::Unknown;
    }

    std::string GameVersionString(GameVersion version)
    {
        switch (version)
        {
        case GameVersion::OG:
            return "Original Game (1.10.163)";
        case GameVersion::NewGen:
            return "Next Generation (1.10.980-1.10.984)";
        case GameVersion::AE:
            return "Anniversary Edition (1.11.240)";
        case GameVersion::Unknown:
        default:
            return "UNKNOWN";
        }
    }

    std::filesystem::path ResolveLogDirectory()
    {
        wchar_t* docs = nullptr;
        if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &docs)) && docs)
        {
            std::filesystem::path dir = docs;
            ::CoTaskMemFree(docs);
            return dir / L"My Games" / L"Fallout4" / L"F4SE";
        }
        if (docs) ::CoTaskMemFree(docs);
        return {};
    }

    void InitializeLogger()
    {
        try
        {
            std::filesystem::path logDir = ResolveLogDirectory();
            if (logDir.empty())
                logDir = "Logs";
            std::filesystem::create_directories(logDir);

            auto logPath = logDir / "FalloutAnomaly.log";
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string());
            logger = std::make_shared<spdlog::logger>("FalloutAnomaly", sink);
        }
        catch (const std::exception&)
        {
            try
            {
                std::filesystem::create_directories("Logs");
                auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs\\FalloutAnomaly.log");
                logger = std::make_shared<spdlog::logger>("FalloutAnomaly", sink);
            }
            catch (const std::exception&)
            {
                logger = std::make_shared<spdlog::logger>("FalloutAnomaly",
                    std::make_shared<spdlog::sinks::null_sink_mt>());
            }
        }

        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::register_logger(logger);

        logger->info("===========================================");
        logger->info("FALLOUT ANOMALY - RADIATION OVERHAUL");
        logger->info("Plugin Version: 1.0");
        logger->info("Build Date: {}", __DATE__);
        logger->info("===========================================");
    }

    void SaveCallback(const F4SE::SerializationInterface* a_intfc)
    {
        RadiationEngine::GetSingleton().SaveState(a_intfc);
        const auto climate = ClimateRuntime::GetSingleton().SnapshotForSave();
        if (!a_intfc->OpenRecord(RadClimateSerialization::kRecord, RadClimateSerialization::kVersion))
        {
            if (auto logger = spdlog::get("FalloutAnomaly"))
                logger->error("[Save] Could not open climate record — temperature and wetness will not persist.");
            return;
        }
        if (!a_intfc->WriteRecordData(climate.coreC) || !a_intfc->WriteRecordData(climate.wetness01))
        {
            if (auto logger = spdlog::get("FalloutAnomaly"))
                logger->error("[Save] Climate record write failed — temperature and wetness may not persist.");
        }
    }

    void LoadCallback(const F4SE::SerializationInterface* a_intfc)
    {
        RadiationEngine::GetSingleton().LoadState(a_intfc);
    }

    void RevertCallback(const F4SE::SerializationInterface* )
    {
        RadiationEngine::GetSingleton().RevertState();
        ClimateRuntime::GetSingleton().Reset();
        NeedsRuntime::GetSingleton().Reset();
        OutbreakRuntime::GetSingleton().Reset();
    }

    std::atomic<bool> g_radWaiting{ false };
    std::atomic<std::uint64_t> g_radLoadGeneration{ 0 };

    bool IsCurrentRadHudWait(std::uint64_t generation)
    {
        return g_radWaiting.load(std::memory_order_acquire) &&
               RadCorrectnessPolicy::IsCurrentLoadGeneration(
                   generation, g_radLoadGeneration.load(std::memory_order_acquire));
    }

    void WaitThenCreateRadHud(std::uint64_t generation)
    {
        auto free = std::make_shared<std::atomic<bool>>(false);
        auto probeQueued = std::make_shared<std::atomic<bool>>(false);
        bool wasFree = false;
        auto freeSince = std::chrono::steady_clock::now();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

        while (IsCurrentRadHudWait(generation)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                if (RadCorrectnessPolicy::IsCurrentLoadGeneration(
                        generation, g_radLoadGeneration.load(std::memory_order_acquire)))
                    g_radWaiting.store(false, std::memory_order_release);
                if (logger) logger->warn("FalloutAnomaly: radhud wait thread timed out (10 min) — giving up this load.");
                break;
            }

            bool expectedProbe = false;
            if (probeQueued->compare_exchange_strong(expectedProbe, true, std::memory_order_acq_rel))
            {
                free->store(false, std::memory_order_release);
                if (!Hooks::QueueMainThreadTask([free, probeQueued, generation]() {
                        probeQueued->store(false, std::memory_order_release);
                        if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                                generation, g_radLoadGeneration.load(std::memory_order_acquire)))
                            return;
                        auto* ui = RE::UI::GetSingleton();
                        auto* cm = RE::ControlMap::GetSingleton();
                        auto* pc = RE::PlayerCharacter::GetSingleton();
                        const bool ctrl = ui && ui->menuMode == 0 && cm && !cm->ignoreKeyboardMouse && pc && pc->parentCell;
                        free->store(ctrl, std::memory_order_release);
                    }))
                {
                    probeQueued->store(false, std::memory_order_release);
                    if (logger) logger->debug("FalloutAnomaly: radhud world-ready probe deferred until the game-thread dispatcher accepts work.");
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!IsCurrentRadHudWait(generation)) break;

            if (free->load(std::memory_order_acquire)) {
                if (!wasFree) {
                    wasFree = true;
                    freeSince = std::chrono::steady_clock::now();
                } else {
                    const auto held = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - freeSince).count();
                    if (held >= 5000) {
                        if (logger) logger->info("FalloutAnomaly: player is in world, creating radhud view");
                        const bool createQueued = Hooks::QueueMainThreadTask([generation]() {
                            if (RadCorrectnessPolicy::IsCurrentLoadGeneration(
                                    generation, g_radLoadGeneration.load(std::memory_order_acquire)))
                            {
                                auto& hud = RadHud::GetSingleton();
                                if (!hud.StartPrismaViewRetry())
                                    hud.CreateView();
                            }
                        });
                        if (!createQueued)
                        {
                            if (logger) logger->warn("FalloutAnomaly: radhud CreateView dispatch deferred; HUD retry worker will retry.");
                            continue;
                        }
                        if (RadCorrectnessPolicy::IsCurrentLoadGeneration(
                                generation, g_radLoadGeneration.load(std::memory_order_acquire)))
                            g_radWaiting.store(false, std::memory_order_release);
                        break;
                    }
                }
            } else {
                wasFree = false;
            }
        }
    }

    void StartRadHudWaitIfIdle(std::uint64_t loadGeneration)
    {
        if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire)))
            return;

        bool expected = false;
        if (g_radWaiting.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            if (!RadCorrectnessPolicy::ShouldStartHudWait(
                    false, loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire)))
            {
                g_radWaiting.store(false, std::memory_order_release);
                return;
            }
            std::thread(WaitThenCreateRadHud, loadGeneration).detach();
        }
    }

    void MessageHandler(F4SE::MessagingInterface::Message* a_msg)
    {
        if (!logger || !a_msg)
        {
            if (logger) logger->error("[MessageHandler] Received null message pointer");
            return;
        }

        logger->debug("[MessageHandler] Processing message type: {} on thread {}",
            static_cast<int>(a_msg->type), ::GetCurrentThreadId());

        switch (a_msg->type)
        {
        case F4SE::MessagingInterface::kGameDataReady:
            logger->info("========== GAME DATA READY EVENT ==========");
            logger->info("Game Version: {}", GameVersionString(g_gameVersion));

            if (!Hooks::InstallMainThreadDispatcher())
            {
                logger->critical("Failed to establish Fallout 4 main-thread dispatch configuration; radiation engine will retry on load events.");
                break;
            }

            if (!Hooks::QueueMainThreadTask([]() {
                    if (!RadLifecycle::EnsureEngineActiveOnGameThread())
                        logger->error("[Lifecycle] GameDataReady activation incomplete; retrying on PostLoad/NewGame.");
                    logger->info("===========================================");
                }))
            {
                logger->warn("[Lifecycle] GameDataReady activation deferred; game-thread dispatcher is not ready yet.");
            }
            break;

        case F4SE::MessagingInterface::kPostLoadGame:
        {
            logger->info("========== POST LOAD GAME EVENT ==========");
            RadLifecycleDispatch::BlockPlayerUpdates();
            const auto loadGeneration = g_radLoadGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
            RadLifecycleDispatch::SetPlayerUpdateGeneration(loadGeneration);

            if (!Hooks::InstallMainThreadDispatcher())
            {
                logger->critical("[PostLoad] No verified game-main-thread dispatcher; Radiation Engine remains INACTIVE.");
                break;
            }

            logger->info("[PostLoad] kPostLoadGame received on thread {}; queuing post-load task.",
                ::GetCurrentThreadId());
            RadLifecycleDispatch::QueueWithRetry(
                &g_radLoadGeneration, loadGeneration, "PostLoad", [loadGeneration]() {
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration,
                            g_radLoadGeneration.load(std::memory_order_acquire)))
                    {
                        logger->debug("[PostLoad] ignoring stale task for load generation {}.", loadGeneration);
                        return;
                    }
                    logger->info("[PostLoad] task entered on game-main thread {}.", ::GetCurrentThreadId());
                    RadHud::GetSingleton().BeginLoadSettle();

                    if (!RadLifecycle::EnsureEngineActiveOnGameThread())
                    {
                        logger->critical("[PostLoad] Game save loaded, but Radiation Engine remains INACTIVE; post-load mutations skipped.");
                        return;
                    }

                    auto& engine = RadiationEngine::GetSingleton();
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    RadHud::GetSingleton().ResetDebugMenuForSession();
                    logger->info("[PostLoad] OnPostLoad begin.");
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    engine.OnPostLoad();
                    logger->info("[PostLoad] OnPostLoad end; ResetAppliedPenalties begin.");
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    engine.ResetAppliedPenalties();
                    logger->info("[PostLoad] ResetAppliedPenalties end; StartRadHudWaitIfIdle begin.");
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    StartRadHudWaitIfIdle(loadGeneration);
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    RadLifecycleDispatch::MarkPlayerUpdateGenerationReady(loadGeneration);
                    logger->info("[PostLoad] StartRadHudWaitIfIdle end; post-load task complete.");
                    logger->info("Game save loaded. Radiation Engine ACTIVE.");
                    logger->info("  - Load-state engine mutation completed on game-main thread");
                    logger->info("=========================================");
                });
            break;
        }

        case F4SE::MessagingInterface::kPostPostLoad:
            logger->info("Post-post load event received");
            break;

        case F4SE::MessagingInterface::kNewGame:
        {
            logger->info("New game started");
            RadLifecycleDispatch::BlockPlayerUpdates();
            const auto loadGeneration = RadCorrectnessPolicy::BeginNewGameLoad(
                g_radLoadGeneration, g_radWaiting);
            RadLifecycleDispatch::SetPlayerUpdateGeneration(loadGeneration);

            if (!Hooks::InstallMainThreadDispatcher())
            {
                logger->critical("[NewGame] No verified game-main-thread dispatcher; Radiation Engine remains INACTIVE.");
                break;
            }

            RadLifecycleDispatch::QueueWithRetry(
                &g_radLoadGeneration, loadGeneration, "NewGame", [loadGeneration]() {
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration,
                            g_radLoadGeneration.load(std::memory_order_acquire)))
                    {
                        logger->debug("[NewGame] ignoring stale task for load generation {}.", loadGeneration);
                        return;
                    }

                    if (!RadLifecycle::EnsureEngineActiveOnGameThread())
                    {
                        logger->critical("[NewGame] Radiation Engine remains INACTIVE; new-game mutations skipped.");
                        return;
                    }

                    auto& engine = RadiationEngine::GetSingleton();
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    RadHud::GetSingleton().ResetDebugMenuForSession();
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    RadHud::GetSingleton().BeginLoadSettle();
                    engine.OnPostLoad();
                    ClimateRuntime::GetSingleton().Reset();
                    NeedsRuntime::GetSingleton().Reset();
                    OutbreakRuntime::GetSingleton().Reset();
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    engine.OnNewGame();
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    engine.ResetAppliedPenalties();
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    StartRadHudWaitIfIdle(loadGeneration);
                    if (!RadCorrectnessPolicy::IsCurrentLoadGeneration(
                            loadGeneration, g_radLoadGeneration.load(std::memory_order_acquire))) return;
                    RadLifecycleDispatch::MarkPlayerUpdateGenerationReady(loadGeneration);
                    logger->debug("  - New-game radiation state reset on game-main thread");
                    logger->debug("  - Update loop: RUNNING");
                });
            break;
        }

        case F4SE::MessagingInterface::kPreLoadGame:
        {
            RadLifecycleDispatch::BlockPlayerUpdates();
            const auto loadGeneration = g_radLoadGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
            RadLifecycleDispatch::SetPlayerUpdateGeneration(loadGeneration);
            g_radWaiting.store(false, std::memory_order_release);
            logger->info("Game loading...");
            break;
        }

        default:
            logger->debug("Unhandled message type: {}", static_cast<int>(a_msg->type));
            break;
        }
    }
}

F4SE_PLUGIN_VERSION = []() noexcept {
    F4SE::PluginVersionData data{};
    data.PluginVersion(REL::Version(1, 0, 0));
    data.PluginName("Fallout Anomaly - Radiation Overhaul");
    data.AuthorName("NomadsReach");
    data.UsesSigScanning(false);
    data.UsesAddressLibrary(true);
    data.UsesAddressLibraryNG(true);
    data.HasNoStructUse(false);
    data.IsLayoutDependent(true);
    data.IsLayoutDependentNG(true);
    data.CompatibleVersions({});
    return data;
}();

extern "C" __declspec(dllexport) bool F4SEAPI
F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
        auto* skse = reinterpret_cast<const F4SE::QueryInterface*>(a_f4se);
        if (!skse)
        {
            return false;
        }

        a_info->infoVersion = F4SE::PluginInfo::kVersion;
        a_info->name = "Fallout Anomaly - Radiation Overhaul";
        a_info->version = 1;

        if (skse->IsEditor())
        {
            return false;
        }

        g_gameVersion = DetectGameVersion(skse);
        return g_gameVersion == GameVersion::OG || g_gameVersion == GameVersion::AE;
}

extern "C" __declspec(dllexport) bool F4SEAPI
F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
        F4SE::Init(a_f4se, { .log = false });

        if (g_gameVersion == GameVersion::Unknown)
        {
            g_gameVersion = DetectGameVersion(a_f4se);
        }

        InitializeLogger();

        if (g_gameVersion != GameVersion::OG && g_gameVersion != GameVersion::AE)
        {
            logger->critical(
                "Unsupported Fallout 4 runtime: {}. Supported runtimes are OG 1.10.163 and AE 1.11.240; "
                "NG 1.10.980-1.10.984 remains unsupported.",
                GameVersionString(g_gameVersion));
            return false;
        }

        logger->info("F4SEPlugin_Load called");
        logger->info("Initializing plugin infrastructure...");
        logger->debug("  - Game Version: {}", GameVersionString(g_gameVersion));
        logger->debug("  - Logger: Initialized");
        logger->debug("  - Spdlog Level: DEBUG (only visible when the level is lowered)");

        auto messaging = F4SE::GetMessagingInterface();
        if (!messaging)
        {
            logger->error("✗ CRITICAL: Failed to get F4SE messaging interface");
            return false;
        }

        logger->debug("  - Messaging Interface: OK");

        if (!messaging->RegisterListener(MessageHandler))
        {
            logger->error("✗ CRITICAL: Failed to register message listener");
            logger->error("  - MessageHandler callback registration failed");
            return false;
        }

        logger->info("✓ Message listener registered successfully");
        logger->debug("  - Will receive: kGameDataReady, kPostLoadGame, kPostPostLoad, kNewGame, kPreLoadGame");

        if (auto* serialization = F4SE::GetSerializationInterface())
        {
            serialization->SetUniqueID('FARO');
            serialization->SetRevertCallback(RevertCallback);
            serialization->SetSaveCallback(SaveCallback);
            serialization->SetLoadCallback(LoadCallback);
            logger->info("✓ Serialization registered — storm clock persists across saves.");
        }
        else
        {
            logger->warn("Serialization interface unavailable — storm clock will NOT persist.");
        }

        RadReliefConfig::InstallPapyrusBridge();
        RadNeedsTuning::Install();
        ClimateEffects::InstallPapyrusBridge();
        OutbreakQuestHeld::Install();

        logger->info("✓ Plugin loaded successfully. Awaiting game events...");

        return true;
}
