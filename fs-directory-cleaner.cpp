// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <print>

namespace fs = std::filesystem;
using namespace std::string_view_literals;

enum class RunMode : bool
{
    DryRun,
    Execute
};

auto const Reset = "\033[0m"sv;
auto const Yellow = "\033[33;1m"sv;
auto const Red = "\033[31;1m"sv;
auto const Green = "\033[32;1m"sv;

{
    if (fs::is_directory(path))
    {
        for (const auto& entry: fs::directory_iterator(path, fs::directory_options::skip_permission_denied))
        {
            deleteRecursively(entry.path(), runMode);
        }
    }
    else
    {
        switch (runMode)
        {
            case RunMode::DryRun:
                std::print("{}Removing (dry-run):{} {}\n", Yellow, Reset, path.string());
                break;
            case RunMode::Execute: {
                std::print("{}Removing:{} {}\n", Red, Reset, path.string());
                auto ec = std::error_code {};
                fs::remove(path, ec);
                if (ec)
                    std::cerr << std::format("Error: {}\n", ec.message());
                break;
            }
        }
    }
}

void deleteDirectoriesIfOlderThan(const fs::path& baseDirectory,
                                  fs::file_time_type::clock::time_point oldestAllowed,
                                  RunMode runMode = RunMode::DryRun)
{
    std::println("Deleting directories older than: {} in: {}", oldestAllowed.time_since_epoch().count(), baseDirectory.string());
    auto const reset = "\033[0m"sv;
    auto const red = "\033[31;1m"sv;
    auto const green = "\033[32;1m"sv;
    for (auto const& entry: fs::directory_iterator(baseDirectory))
    {
        auto ec = std::error_code {};
        auto const lastWriteTime = entry.last_write_time();
        if (lastWriteTime.time_since_epoch() < oldestAllowed.time_since_epoch())
        {
            std::println("{}Deleting:{} {}", red, reset, entry.path().string());
            deleteRecursively(entry, runMode);
        }
        else
            std::println("{}Skipping:{} {}", Green, Reset, entry.path().string());
    }
}

int main(int argc, char** argv)
{
    // TODO: allow running this script in the background (daemon mode on Unix, Service on Windows) and
    //       have it delete directories older than a certain age every N minutes.
    //
    // TODO: Add a command line option to specify the interval at which to run deletion.
    // TODO: Add a command line option to specify the run mode (dry-run, execute).

    if (argc != 3)
    {
        std::cerr << "Usage: fs-directory-cleaner <root-path> <minimum-age-in-minutes>\n";
        return EXIT_FAILURE;
    }

    auto const rootPath = fs::path(argv[1]);
    auto const minimumAge = std::chrono::minutes(std::stoi(argv[2]));
    auto const now = fs::file_time_type::clock::now();
    auto const oldestAllowed = now - minimumAge;

    deleteDirectoriesIfOlderThan(rootPath, oldestAllowed, RunMode::DryRun);

    return EXIT_SUCCESS;
}
