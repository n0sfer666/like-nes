#include "platform_env.hpp"
#include "platform_redact.hpp"

#include <cstdio>
#include <string>

// Аудит #21 A·3·12: путь каталога сейва и бандла печатался в stdout/stderr целиком, вместе с именем
// учётной записи ОС. Обе семантики сравнения проверяются на каждой ОС — правила передаются
// параметром; перегрузка по окружению — только своей ОС.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "[platform-redact] FAIL: %s\n", what);
        ++fails;
    }
}

void check_posix() {
    const auto r = [](const char* p, const char* h) {
        return platform::redact_home(p, h, platform::PathRules::Posix);
    };
    check(r("/Users/x/Library/Application Support/like-nes", "/Users/x") ==
              "~/Library/Application Support/like-nes", "home prefix becomes ~");
    check(r("/Users/x/save", "/Users/x/") == "~/save", "trailing slash of home is ignored");
    check(r("/Users/x", "/Users/x") == "~", "home itself becomes ~");
    check(r("/Users/x/", "/Users/x") == "~/", "home with a trailing slash becomes ~/");
    check(r("/Users/xy/save", "/Users/x") == "/Users/xy/save", "prefix must be a whole component");
    check(r("/tmp/save", "/Users/x") == "/tmp/save", "path outside home is kept");
    check(r("/tmp/save", "") == "/tmp/save", "empty home redacts nothing");
    check(r("/tmp/save", "/") == "/tmp/save", "root home redacts nothing");
    check(r("/Users/X/save", "/Users/x") == "/Users/X/save", "POSIX paths are case-sensitive");
    check(r("/Users/x\\save", "/Users/x") == "/Users/x\\save", "backslash is a POSIX name character");
}

void check_windows() {
    const auto r = [](const char* p, const char* h) {
        return platform::redact_home(p, h, platform::PathRules::Windows);
    };
    check(r("C:\\Users\\x\\AppData\\Roaming\\like-nes", "C:\\Users\\x") ==
              "~\\AppData\\Roaming\\like-nes", "Windows separators");
    check(r("c:\\users\\X\\save", "C:\\Users\\x") == "~\\save", "Windows ignores ASCII case");
    check(r("C:/Users/x/save", "C:\\Users\\x\\") == "~/save", "slash and backslash are one separator");
    check(r("C:\\Users\\xy\\save", "C:\\Users\\x") == "C:\\Users\\xy\\save",
          "Windows prefix must be a whole component");
    check(r("C:\\save", "C:\\") == "C:\\save", "drive root home redacts nothing");
}

void check_process_home() {
    const char* vars[] = {"HOME", "USERPROFILE"};
    std::string saved[2];
    bool had[2];
    for (int i = 0; i < 2; ++i) {
        had[i] = platform::env_var(vars[i], saved[i]);
        check(platform::env_put(vars[i], "/home/probe"), "setting the home variable");
    }
    check(platform::redact_home("/home/probe/.local/share/like-nes") == "~/.local/share/like-nes",
          "process home is redacted");
    for (int i = 0; i < 2; ++i)
        check(platform::env_put(vars[i], had[i] ? saved[i].c_str() : nullptr), "restoring the home variable");
}

} // namespace

int main() {
    check_posix();
    check_windows();
    check_process_home();
    std::printf("platform-redact: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
