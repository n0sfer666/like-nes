#include "save.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#include "platform_args.hpp"
#include "platform_fs.hpp"

namespace {
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-atomic: FAIL %s\n", why); } }
}

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc == 3 && std::strcmp(argv[1], "--write") == 0) {
        const uint8_t bytes[]{9, 8, 7};
        return save::write_atomic(argv[2], bytes) ? 0 : 1;
    }
    const std::string path = "save_atomic_\xd1\x82\xd0\xb5\xd1\x81\xd1\x82.bin";
    const std::vector<uint8_t> big(8192, 0xa5), small{0, 1, 0, 255};
    std::vector<uint8_t> got;
    check(save::write_atomic(path, big), "large durable write with UTF-8 path");
    check(platform::read_bytes(path, got) && got == big, "full large image");
    check(platform::write_atomic(path + ".tmp", big), "seed stale temp");
    check(save::write_atomic(path, small), "replace over stale temp");
    check(platform::read_bytes(path, got) && got == small, "short image has no trailing old bytes");
    check(!platform::file_exists(path + ".tmp"), "temp removed after rename");
    const std::string nul_path = path + std::string(1, '\0') + "ignored";
    check(!save::write_atomic(nul_path, big), "embedded NUL rejected before open");
    check(platform::read_bytes(path, got) && got == small, "invalid path cannot truncate the target");
    check(save::write_atomic(path, {}), "empty payload");
    check(platform::read_bytes(path, got) && got.empty(), "empty replaces old payload");
    check(!save::write_atomic("", small), "empty path rejected");
    check(!save::write_atomic(path + "/child", small), "non-directory parent fails");
    check(platform::read_bytes(path, got) && got.empty(), "failure preserves prior target");
    check(!platform::sync_file(nullptr), "null sync handle rejected");
    platform::remove_file(path); platform::remove_file(path + ".tmp");
    std::puts(fails ? "save-atomic: FAIL" : "save-atomic: PASS overwrite, empty, UTF-8, stale temp, failed parent");
    return fails ? 1 : 0;
}
