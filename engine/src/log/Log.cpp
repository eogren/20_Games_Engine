#include "log/Log.h"

#include <spdlog/sinks/rotating_file_sink.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#endif

#include <memory>
#include <vector>

namespace engine::log
{

    void init(const Config& cfg)
    {
        std::vector<spdlog::sink_ptr> sinks;

#ifdef _WIN32
        // OutputDebugStringW under the hood. Cheap when no debugger is
        // listening — Windows just discards the message.
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

        // rotating_file_sink_mt opens the file at construction; if the
        // path's parent doesn't exist it throws. We let that propagate
        // — startup logging failure should surface loudly, not be
        // silently routed only to the debugger sink.
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            cfg.file_path.string(), cfg.max_file_bytes, cfg.max_files));

        auto logger = std::make_shared<spdlog::logger>(
            "engine", sinks.begin(), sinks.end());

        logger->set_level(cfg.level);

        // info-level lines are batched (cheap); warn and above flush
        // immediately so a crash right after the log call still leaves
        // the line on disk.
        logger->flush_on(spdlog::level::warn);

        // [2026-05-13 14:22:01.123] [info] message
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        spdlog::set_default_logger(std::move(logger));
    }

    void shutdown()
    {
        spdlog::shutdown();
    }

} // namespace engine::log
