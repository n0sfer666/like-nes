#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bakers.hpp"
#include "bundle_writer.hpp"
#include "codec.hpp"
#include "format.hpp"
#include "platform_args.hpp"
#include "validate_materials.hpp"
#include "verify_game.hpp"

// Headless CLI-бейкер (спека #5): source (png+wgsl+bulk) → детерм. bundle.
// Один код бейка (CI + IDE-watch поверх). Печатает golden bundle_hash (гейт #1).

using namespace asset;

namespace {

int emit(const char* what, const std::string& path, std::vector<AssetInput> assets) {
    std::vector<uint8_t> bundle = write_bundle(std::move(assets));
    // Пустой вывод — сигнал guard'а writer'а (бандл не влез в uint32-адресацию). Без проверки
    // на диск лёг бы 0-байтный файл, а заголовок читался бы по nullptr.
    if (bundle.empty()) {
        std::fprintf(stderr, "[assetc] bundle does not fit uint32 addressing: %s\n", path.c_str());
        return 1;
    }
    if (!codec::write_file(path, bundle)) {
        std::fprintf(stderr, "[assetc] write failed: %s\n", path.c_str());
        return 1;
    }
    const BundleHeader* h = reinterpret_cast<const BundleHeader*>(bundle.data());
    std::printf("[assetc] %s %s (%u bytes, %u assets)\n", what, path.c_str(), h->total_size,
                h->asset_count);
    std::printf("[assetc] bundle_hash = 0x%016llx\n", (unsigned long long)h->bundle_hash);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: assetc <src-dir> <out.bundle> [--tint P] [--basisu P]\n"
                     "       assetc --synthetic <out.bundle>  (tools-free, for CI)\n"
                     "       assetc --verify-game <src-dir> <bundle>  (tools-free, for CI)\n"
                     "       assetc --materials <library.mat> <effects.wgsl> <out.bundle>  (tools-free)\n");
        return 2;
    }
    // Сверка закоммиченного бандла с исходниками. Отдельный режим, а не флаг бейка: перепечь
    // `--game` без tint/basisu нельзя, а сверить tool-free секции можно везде.
    if (std::strcmp(argv[1], "--verify-game") == 0) {
        if (argc < 4) {
            std::fprintf(stderr, "usage: assetc --verify-game <src-dir> <bundle>\n");
            return 2;
        }
        return verify_game_bundle(argv[2], argv[3]) ? 0 : 1;
    }
    // Tools-free синтетический бейк (CI): writer+zstd, без tint/basisu.
    if (std::strcmp(argv[1], "--synthetic") == 0) {
        std::vector<AssetInput> synth;
        bakers::synthetic(synth);
        return emit("synthetic", argv[2], std::move(synth));
    }
    // Аудио-бандл (спека #3): committed .ogg → Mmap/Vorbis. Отдельный бандл — golden-хеши
    // рендер/ассет-бейков не трогаются.
    if (std::strcmp(argv[1], "--audio") == 0) {
        if (argc < 4) {
            std::fprintf(stderr, "usage: assetc --audio <src-dir> <out.bundle>\n");
            return 2;
        }
        std::string asrc = argv[2];
        std::vector<AssetInput> assets;
        bool ok = bakers::audio(asrc + "/sfx_ping.ogg", "sfx_ping", false, assets);
        ok = ok && bakers::audio(asrc + "/music.ogg", "music", true, assets);
        if (!ok) return 1;
        return emit("audio", argv[3], std::move(assets));
    }

    // Библиотека эффектов (спека #18): `library.mat` → секция `materials` отдельным бандлом.
    // Отдельным, а не внутри `--game`: библиотека принадлежит ДВИЖКУ (решение 4), её потребитель
    // номер один — golden-харнесс рендера, и класть её в бандл игры-образца значило бы, что гейт
    // движка не запускается без ассетов примера. Пекарь чистый, внешних кодеков не зовёт, поэтому
    // режим работает на любой машине, включая раннеры без tint и basisu.
    if (std::strcmp(argv[1], "--materials") == 0) {
        if (argc < 5) {
            std::fprintf(stderr,
                         "usage: assetc --materials <library.mat> <effects.wgsl> <out.bundle>\n");
            return 2;
        }
        std::vector<AssetInput> massets;
        if (!bakers::materials(argv[2], argv[3], massets)) return 1;
        // Валидация стоит МЕЖДУ бейком и записью: отвергнутая библиотека не должна оставлять на
        // диске бандл, которым потом кто-то нарисует кадр (гейт 2 спеки #18).
        if (!validate_materials(massets, argv[3])) return 1;
        return emit("materials", argv[4], std::move(massets));
    }

    // Игра-образец (спека #8 шов assetc→билд): плейсхолдер-atlas.png → target-native
    // UASTC bake → game.bundle. Рантайм игры транскодит UASTC→BC7 и грузит из бандла
    // (не из процедурного кода). Отдельный бандл — golden-хеши прочих бейков не трогаются.
    if (std::strcmp(argv[1], "--game") == 0) {
        if (argc < 4) {
            std::fprintf(stderr, "usage: assetc --game <src-dir> <out.bundle> [--basisu P]\n");
            return 2;
        }
        std::string gsrc = argv[2], gout = argv[3];
        codec::Tools gtools{"tint", "basisu"};
        for (int i = 4; i + 1 < argc; i += 2)
            if (std::strcmp(argv[i], "--basisu") == 0) gtools.basisu = argv[i + 1];
        std::vector<AssetInput> gassets;
        const std::string gtmp = gout + ".ktx2.tmp";
        if (!bakers::texture(gtools, gsrc + "/atlas.png", "atlas", gtmp, gassets)) return 1;
        if (!bakers::achievements(gsrc + "/achievements.txt", gassets)) return 1;
        if (!bakers::input_presets(gsrc + "/input.txt", gassets)) return 1;
        if (!bakers::movement(gsrc + "/movement.txt", gassets)) return 1;
        if (!bakers::tilemap(gsrc + "/tilemap.txt", gassets)) return 1;
        if (!bakers::atlas_regions(gsrc + "/atlas.txt", gassets)) return 1;
        return emit("game", gout, std::move(gassets));
    }

    std::string src = argv[1];
    std::string out_path = argv[2];
    codec::Tools tools{"tint", "basisu"};
    for (int i = 3; i + 1 < argc; i += 2) {
        if (std::strcmp(argv[i], "--tint") == 0) tools.tint = argv[i + 1];
        else if (std::strcmp(argv[i], "--basisu") == 0) tools.basisu = argv[i + 1];
    }

    std::vector<AssetInput> assets;
    const std::string tmp = out_path + ".ktx2.tmp";
    bool ok = true;
    ok = ok && bakers::texture(tools, src + "/hero_albedo.png", "hero_albedo", tmp, assets);
    ok = ok && bakers::texture(tools, src + "/hero_normal.png", "hero_normal", tmp, assets);
    ok = ok && bakers::shader(tools, src + "/sprite.wgsl", "sprite.vs", "vs_main", 0, assets);
    ok = ok && bakers::shader(tools, src + "/sprite.wgsl", "sprite.fs", "fs_main", 1, assets);
    if (!ok) return 1;
    bakers::bulk("scene_bulk", assets);

    return emit("wrote", out_path, std::move(assets));
}
