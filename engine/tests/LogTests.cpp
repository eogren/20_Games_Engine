// Smoke-level coverage for the log facade. We don't try to test
// spdlog itself — these checks just confirm the multi-sink default
// logger is installed and that a log call reaches at least one sink
// without throwing.

#include "doctest.h"

#include "log/Log.h"

#include <spdlog/spdlog.h>

#include <filesystem>

TEST_CASE("engine::log::init installs a default logger and accepts log calls")
{
    const auto log_path = std::filesystem::temp_directory_path() / "engine_log_test.log";
    std::filesystem::remove(log_path);

    engine::log::init({.file_path = log_path,
                       .max_file_bytes = 64u * 1024u,
                       .max_files = 2,
                       .level = spdlog::level::trace});

    REQUIRE(spdlog::default_logger() != nullptr);
    CHECK(spdlog::default_logger()->name() == "engine");
    CHECK(spdlog::default_logger()->level() == spdlog::level::trace);

    spdlog::info("log smoke test - info");
    spdlog::warn("log smoke test - warn");

    spdlog::default_logger()->flush();

    CHECK(std::filesystem::exists(log_path));
    CHECK(std::filesystem::file_size(log_path) > 0);

    engine::log::shutdown();
    std::filesystem::remove(log_path);
}
