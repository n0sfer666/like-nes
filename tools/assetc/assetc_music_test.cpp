#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#include "assetc_manifest.hpp"
#include "assetc_manifest_mode.hpp"
#include "baker_guid.hpp"
#include "manifest_bake.hpp"
#include "music_read.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"

namespace {
using namespace asset;
int failures;
void check(bool ok, const std::string& why) { if (!ok) { ++failures; std::printf("assetc-music: FAIL %s\n", why.c_str()); } }
void write(const std::string& path, const std::string& text) { check(codec::write_file(path, {text.begin(), text.end()}), "write " + path); }
std::vector<uint8_t> fixture(const char* name) {
    std::vector<uint8_t> bytes;
    check(platform::read_bytes(std::string(MUSIC_FIXTURE_ROOT) + "/" + name, bytes) && !bytes.empty(), "read committed Ogg");
    return bytes;
}
void grammar() {
    std::vector<manifest::Record> r; manifest::Error e;
    for (const char* suffix : {"", "|fade_ms=750", "|37|4800", "|37|4800|fade_ms=0"}) {
        check(manifest::parse(std::string("music|menu|vorbis|song.ogg") + suffix, r, e) && r.size() == 1, "valid music grammar");
        if (r.size() == 1) {
            check(r[0].music.fade_ms == (std::strstr(suffix, "750") ? 750u : std::strstr(suffix, "=0") ? 0u : 1000u), "suffix reaches record");
            check(r[0].music.loop_start == (std::strstr(suffix, "37") ? 37u : 0u), "loop start reaches record");
            check(r[0].music.loop_end == (std::strstr(suffix, "37") ? std::optional<uint32_t>(4800) : std::nullopt), "optional loop end preserved");
        }
    }
    for (const char* line : {"music", "music|x", "music|x|vorbis", "music|x|opus|x.ogg", "music|x|vorbis|x.wav",
        "music|x|vorbis|../x.ogg", "music|x|vorbis|x.ogg|12", "music|x|vorbis|x.ogg|0|0", "music|x|vorbis|x.ogg|3|2",
        "music|x|vorbis|x.ogg|-1|2", "music|x|vorbis|x.ogg|1.5|2", "music|x|vorbis|x.ogg|4294967296|2",
        "music|x|vorbis|x.ogg|fade_ms=", "music|x|vorbis|x.ogg|fade_ms=-1", "music|x|vorbis|x.ogg|fade_ms=89478486",
        "music|x|vorbis|x.ogg|fade_ms=1|fade_ms=2", "music|x|vorbis|x.ogg|fade_ms=1|0|2", "music|x|vorbis|x.ogg|fade=1",
        "music|x|vorbis|x.ogg|0|2|fade_ms=1|extra", "music|music|vorbis|x.ogg",
        "texture|music|pixel|x.png\nmusic|x|vorbis|x.ogg", "music|x|vorbis|x.ogg\ntexture|music|pixel|x.png"})
        check(!manifest::parse(line, r, e) && !e.message.empty(), std::string("bad grammar: ") + line);
    check(!manifest::parse("music|" + std::string(129, 'x') + "|vorbis|x.ogg", r, e), "long name refused");
    check(manifest::parse("texture|music|pixel|x.png", r, e), "old music-named asset stays valid without music records");
    check(manifest::parse("music|x|vorbis|x.ogg|fade_ms=89478485", r, e), "largest representable fade accepted");
}
bool bake(const std::string& root, const std::string& text, manifest::Bake& b) {
    write(root + "/game.manifest", text); b = {}; b.manifest = root + "/game.manifest";
    return manifest::bake(b);
}
void baked(const std::string& root, const std::vector<uint8_t>& source) {
    check(codec::write_file(root + "/song.ogg", source), "copy stereo Ogg");
    manifest::Bake b;
    const std::string text = "music|menu|vorbis|song.ogg|37|4000|fade_ms=750\nmusic|level|vorbis|song.ogg\n";
    check(bake(root, text, b), "bake actual Ogg: " + b.error);
    check(b.deps == std::vector<std::string>{root + "/game.manifest", root + "/song.ogg", root + "/song.ogg"} && b.assets.size() == 3,
          "actual file deps, two untouched Ogg assets and one table");
    framework::music::MusicTable table;
    for (const auto& asset : b.assets) {
        if (asset.guid == bakers::guid_of("music")) {
            check(asset.type == AssetType::Raw && asset.codec == Codec::Raw && asset.residency == Residency::Mmap &&
                table.open(asset.payload.data(), asset.payload.size()) && table.count() == 2, "named raw LNMU");
        } else check(asset.type == AssetType::Audio && asset.codec == Codec::Vorbis && asset.residency == Residency::Mmap &&
            asset.payload == source && asset.uncompressed_size == source.size() && asset.tex_w == 48000 && asset.tex_h == 2 &&
            asset.tex_format == 4800 && asset.variant_key == AUDIO_FLAG_LOOP, "original encoded bytes and inspected audio metadata");
    }
    const auto* menu = table.find("menu"); const auto* level = table.find("level");
    check(menu && menu->guid == bakers::guid_of("menu") && menu->frames == 4800 && menu->channels == 2 &&
        menu->loop_start == 37 && menu->loop_end == 4000 && menu->fade_samples == 36000, "explicit metadata reached table");
    check(level && level->loop_start == 0 && level->loop_end == 4800 && level->fade_samples == 48000, "default metadata reached table");
    const auto before = write_bundle(b.assets);
    check(bake(root, "music|level|vorbis|song.ogg\nmusic|menu|vorbis|song.ogg|37|4000|fade_ms=750\n", b) &&
        write_bundle(b.assets) == before, "canonical bundle independent of record order");
    check(!bake(root, "# loop\nmusic|menu|vorbis|song.ogg|0|4801\n", b) && b.error.find("game.manifest:2:") != std::string::npos &&
        b.error.find("loop bounds") != std::string::npos, "actual loop end refused with source line");
    for (const auto& bad : std::vector<std::pair<const char*, const char*>>{
        {"tests/audio/fixtures/stereo-44100.ogg", "48000 Hz"}, {"tests/audio/fixtures/three-48000.ogg", "mono or stereo"}}) {
        check(codec::write_file(root + "/song.ogg", fixture(bad.first)), "copy unsupported Ogg");
        check(!bake(root, "music|menu|vorbis|song.ogg", b) && b.error.find(bad.second) != std::string::npos, "actual rate/channels refused: " + b.error);
    }
    check(codec::write_file(root + "/song.ogg", fixture("engine/audio/assets/src/sfx_ping.ogg")), "copy mono Ogg");
    check(bake(root, "music|menu|vorbis|song.ogg|fade_ms=0", b) && b.assets[0].tex_h == 1 && b.assets[0].tex_format == 14400, "real mono import and instant fade");
    write(root + "/song.ogg", "not an Ogg");
    check(!bake(root, "music|menu|vorbis|song.ogg", b) && b.error.find("Vorbis") != std::string::npos, "invalid container refused");
    check(codec::write_file(root + "/song.ogg", {source.begin(), source.begin() + source.size() / 2}), "copy truncated Ogg");
    check(!bake(root, "music|menu|vorbis|song.ogg", b), "truncated container refused");
    check(codec::write_file(root + "/song.ogg", std::vector<uint8_t>((64u << 20) + 1)), "oversized file fixture");
    check(!bake(root, "music|menu|vorbis|song.ogg", b) && b.error.find("over 64 MiB") != std::string::npos, "capped read before decoder");
}
void cli(const std::string& root, const std::vector<uint8_t>& source) {
    check(codec::write_file(root + "/song.ogg", source), "restore Ogg");
    write(root + "/game.manifest", "music|menu|vorbis|song.ogg|fade_ms=750");
    const auto out = root + "/out.bundle", dep = root + "/out.d";
    auto invoke = [&](bool pass, const char* diagnostic) {
        std::string log; platform::ExitStatus status;
        check(platform::run_capture({platform::exe_path(), "--manifest", root + "/game.manifest", out, "--depfile", dep}, log, status) &&
            status.kind == platform::ExitKind::Exited && status.code == (pass ? 0 : 1) && log.find(diagnostic) != std::string::npos,
            std::string("CLI result: ") + diagnostic + "\n" + log);
    };
    invoke(true, "music menu: frames=4800 channels=2 loop=0..4800 fade_samples=36000");
    std::vector<uint8_t> before, after; std::string deps, after_deps;
    check(platform::read_bytes(out, before) && !before.empty() && platform::read_text(dep, deps) && deps.find("song.ogg") != std::string::npos, "CLI bundle and actual Ogg depfile");
    check(codec::write_file(root + "/song.ogg", fixture("tests/audio/fixtures/stereo-44100.ogg")), "copy wrong rate");
    invoke(false, "48000 Hz");
    check(platform::read_bytes(out, after) && after == before && platform::read_text(dep, after_deps) && after_deps == deps, "refusal preserves both existing outputs");
    platform::remove_file(out); platform::remove_file(dep);
}
}
int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "--manifest") == 0) return asset::manifest::run_manifest(argc, argv, {});
    const auto root = platform::exe_dir() + "/assetc_music_" + std::to_string(platform::process_id()) + "/\xD0\xBC\xD1\x83\xD0\xB7\xD1\x8B\xD0\xBA\xD0\xB0 test";
    if (!platform::ensure_dir(root)) return 3;
    const auto source = fixture("tests/audio/fixtures/stereo-48000.ogg");
    if (source.empty()) return 3;
    grammar(); baked(root, source); cli(root, source);
    platform::remove_file(root + "/game.manifest"); platform::remove_file(root + "/song.ogg");
    std::puts(failures ? "assetc-music: FAIL" : "assetc-music: PASS grammar, actual Ogg, loops, fades, canonical bytes and CLI refusal");
    return failures ? 1 : 0;
}
