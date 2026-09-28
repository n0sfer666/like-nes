#include "apk_assets.hpp"

#include <android/log.h>

#include <cstdio>
#include <cstring>

#include "platform_fs.hpp"

namespace game {
namespace {

bool unpack_one(AAssetManager* am, const char* name, const std::string& dir) {
    AAsset* in = AAssetManager_open(am, name, AASSET_MODE_BUFFER);
    if (!in) return false;
    const void* data = AAsset_getBuffer(in);
    const size_t len = static_cast<size_t>(AAsset_getLength(in));
    std::FILE* out = platform::open_file(dir + "/" + name, "wb");
    const bool ok = data && out && std::fwrite(data, 1, len, out) == len;
    if (out) std::fclose(out);
    AAsset_close(in);
    return ok;
}

} // namespace

bool unpack_apk_assets(AAssetManager* am, const std::string& dir) {
    if (!am || !platform::ensure_dir(dir)) return false;
    bool game = true;
    for (const char* name : {"game.bundle", "library.bundle", "audio.bundle"}) {
        if (unpack_one(am, name, dir)) continue;
        __android_log_print(ANDROID_LOG_ERROR, "like-nes", "asset unpack failed: %s", name);
        if (std::strcmp(name, "game.bundle") == 0) game = false;
    }
    return game;
}

} // namespace game
