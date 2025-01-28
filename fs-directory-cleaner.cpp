// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <print>
#include <vector>

namespace fs = std::filesystem;
using namespace std::string_view_literals;

class Executor
{
  public:
    Executor() = default;
    Executor(Executor&&) = default;
    Executor& operator=(Executor&&) = default;
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    virtual ~Executor() = default;

    virtual bool isDirectory(fs::path const& path) = 0;
    virtual bool removeFile(fs::path const& path, std::error_code& ec) = 0;
    virtual std::vector<fs::directory_entry> listDirectories(const fs::path& path) = 0;
};

class RealFilesystemExecutor: public Executor
{
  public:
    bool isDirectory(fs::path const& path) override
    {
        return fs::is_directory(path);
    }

    bool removeFile(fs::path const& path, std::error_code& ec) override
    {
        return fs::remove(path, ec);
    }

    std::vector<fs::directory_entry> listDirectories(const fs::path& path) override
    {
        auto result = std::vector<fs::directory_entry>();
        for (auto entry: fs::directory_iterator(path, fs::directory_options::skip_permission_denied))
            result.emplace_back(std::move(entry));
        return result;
    }
};

enum class RunMode : bool
{
    DryRun,
    Execute
};

auto const Reset = "\033[0m"sv;
auto const Yellow = "\033[33;1m"sv;
auto const Red = "\033[31;1m"sv;
auto const Green = "\033[32;1m"sv;

void deleteRecursively(const fs::path& path, RunMode runMode, Executor& executor)
{
    if (executor.isDirectory(path))
    {
        for (const auto& entry: executor.listDirectories(path))
        {
            deleteRecursively(entry.path(), runMode, executor);
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
                executor.removeFile(path, ec);
                if (ec)
                    std::cerr << std::format("Error: {}\n", ec.message());
                break;
            }
        }
    }
}

void deleteDirectoriesIfOlderThan(const fs::path& baseDirectory,
                                  fs::file_time_type::clock::time_point oldestAllowed,
                                  RunMode runMode,
                                  Executor& executor)
{
    for (auto const& entry: executor.listDirectories(baseDirectory))
    {
        auto ec = std::error_code {};
        auto const lastWriteTime = entry.last_write_time();
        if (lastWriteTime.time_since_epoch() < oldestAllowed.time_since_epoch())
            deleteRecursively(entry, runMode, executor);
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
    auto const runMode = RunMode::DryRun;
    auto executor = RealFilesystemExecutor {};

    deleteDirectoriesIfOlderThan(rootPath, oldestAllowed, runMode, executor);

    return EXIT_SUCCESS;
}
