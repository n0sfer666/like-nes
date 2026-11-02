#include "platform_save.hpp"
#include "platform_fs.hpp"

#include <cstdio>

int main() {
    int fails = 0;
    auto check = [&](bool good) { if (!good) ++fails; };
    check(platform::save_dir_override() == nullptr);
    check(!platform::set_save_dir("") && platform::save_dir_override() && platform::save_dir_override()->empty());
    check(!platform::set_save_dir("relative") && platform::save_dir_override()->empty());
    check(!platform::set_save_dir(std::string("/valid\0hidden", 13)) && platform::save_dir_override()->empty());
    // A missing directory is valid here: init of the persistence layer owns creation/failure.
    const std::string absolute = platform::exe_dir() + "/platform-save-not-created";
    check(platform::set_save_dir(absolute) && *platform::save_dir_override() == absolute);
    check(platform::set_save_dir(*platform::save_dir_override()) && *platform::save_dir_override() == absolute);
    check(!platform::set_save_dir("invalid-again") && platform::save_dir_override()->empty());
    std::puts(fails ? "platform-save: FAIL" : "platform-save: PASS unset, explicit refusal and absolute override");
    return fails ? 1 : 0;
}
