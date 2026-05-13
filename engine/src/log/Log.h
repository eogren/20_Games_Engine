// Engine-wide logging entry point. Wraps spdlog just thinly enough to
// own the sink configuration in one place; once init() runs, the rest
// of the codebase uses spdlog's free functions (`spdlog::info(...)`)
// or its `SPDLOG_*` macros (which carry source location) directly.
//
// Two sinks are configured by default:
//   * msvc_sink_mt          - OutputDebugStringW on Windows, visible in
//                             the Visual Studio Output pane and tools
//                             like DebugView. No-op on non-Windows.
//   * rotating_file_sink_mt - durable log on disk; survives crashes and
//                             runs without a debugger attached.
//
// The pair is intentional: msvc_sink covers live developer use; the
// file sink covers post-mortem and headless runs. Drop the file sink
// later if it ever becomes noise.

#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>

namespace engine::log
{

    struct Config
    {
        // Path is relative to the process CWD if not absolute. For a
        // single-game-per-exe setup that's effectively next to the
        // executable, which is fine for dev. Games that want per-user
        // log directories can pass an absolute path under
        // %LOCALAPPDATA% or equivalent.
        std::filesystem::path file_path = "engine.log";

        // 5 MiB * 3 files = 15 MiB max disk footprint. spdlog handles
        // rotation when a write would push the active file past
        // max_file_bytes: foo.log -> foo.1.log, etc.
        std::size_t max_file_bytes = 5u * 1024u * 1024u;
        std::size_t max_files = 3;

        // Minimum severity that reaches any sink. Per-sink filtering
        // is possible but unused today — both sinks log at this level.
        spdlog::level::level_enum level = spdlog::level::info;
    };

    // Installs the multi-sink logger as spdlog's default logger. Safe
    // to call once at startup; calling again replaces the default
    // logger (the old one is dropped by spdlog's registry).
    void init(const Config& cfg = {});

    // Flushes all sinks and drops the registry. spdlog already
    // registers an at-exit hook that does the equivalent, so calling
    // this is only useful before an abort() / std::terminate() path
    // where the at-exit hook won't fire.
    void shutdown();

} // namespace engine::log
