#include "platform_path.hpp"
#include <cstdio>

int main() {
    int fails = 0;
    auto check = [&](const std::string& path, const std::string& expected) {
        if (platform::parent_dir(path) != expected) { ++fails; std::printf("platform-path: FAIL parent '%s'\n", path.c_str()); }
    };
    check("", ""); check("file", "."); check("a/file", "a"); check("/file", "/");
    check("/", "/"); check("/a/file", "/a"); check("a/b/", "a/b");
    if (platform::is_sep('\\')) {
        check("C:\\file", "C:\\"); check("C:save", "C:"); check("C:/a/file", "C:/a");
        check("\\\\server\\share\\file", "\\\\server\\share\\");
    } else {
        check("dir\\name", "."); check("/a\\b", "/");
    }
    std::puts(fails ? "platform-path: FAIL" : "platform-path: PASS syntactic parent, roots and native separators");
    return fails ? 1 : 0;
}
