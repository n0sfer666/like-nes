#include "save_io.hpp"
#include "platform_fs.hpp"
#include "platform_tmpfile_test.hpp"

#include <cstdio>

namespace {
int fails, mode;
bool original, backup, durable;
std::string calls;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-move: FAIL %s\n", why); } }
}
namespace platform {
bool copy_file(const std::string&, const std::string&) {
    calls += "overwrite;"; backup = true; return true;
}
bool copy_file_new(const std::string& from, const std::string& to) {
    calls += "copy;"; check(from == "root/from" && to == "root/to", "exclusive copy paths");
    if (mode == 1 || mode == 2) return false;
    backup = true; return true;
}
bool file_exists(const std::string& path) { check(path == "root/to", "checks collision target"); return backup; }
bool is_dir(const std::string&) { return false; }
std::FILE* open_file(const std::string& path, const char*) {
    calls += "open;"; check(path == "root/to", "sync only own new copy");
    return mode == 3 ? nullptr : test::temporary_file();
}
bool sync_file(std::FILE*) { calls += "sync;"; return mode != 4; }
bool sync_dir_of(const std::string& path) {
    calls += "directory;";
    if (path == "root/to") { durable = mode != 5; return durable; }
    check(path == "root/from" && durable && !original, "final metadata sync only after durable backup/unlink");
    return mode != 7;
}
bool remove_file(const std::string& path) {
    if (path == "root/to") { calls += "cleanup;"; backup = false; return true; }
    calls += "unlink;";
    check(path == "root/from" && durable && backup, "cannot remove original before durable new copy");
    if (mode == 6) return false;
    original = false; return true;
}
}

int main() {
    using save::MoveResult;
    for (mode = 0; mode <= 7; ++mode) {
        original = true; backup = mode == 1; durable = false; calls.clear();
        const auto result = save::detail::move_new_paths("root/from", "root/to");
        const auto expected = mode == 0 || mode == 7 ? MoveResult::Moved : mode == 1 ? MoveResult::Exists : MoveResult::Error;
        check(result == expected, "failure classified by phase");
        if (result == MoveResult::Moved) check(!original && backup && durable, "success has durable backup");
        else check(original, "every Error/Exists preserves original");
        if (mode >= 3 && mode <= 5) check(!backup, "failed publication cleans only new copy");
        if (mode == 6) check(backup && durable, "failed unlink retains durable backup too");
        const char* steps[]{"copy;open;sync;directory;unlink;directory;", "copy;", "copy;", "copy;open;cleanup;",
            "copy;open;sync;cleanup;", "copy;open;sync;directory;cleanup;", "copy;open;sync;directory;unlink;",
            "copy;open;sync;directory;unlink;directory;"};
        check(calls == steps[mode], "ordered publication and error exit");
    }
    std::puts(fails ? "save-move: FAIL" : "save-move: PASS no overwrite, failure preservation and durable-before-unlink");
    return fails ? 1 : 0;
}
