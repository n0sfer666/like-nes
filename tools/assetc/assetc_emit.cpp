#include "assetc_emit.hpp"

#include <cstdio>

#include "codec.hpp"
#include "format.hpp"

namespace asset {

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
    std::printf("[assetc] bundle_hash = 0x%016llx\n", static_cast<unsigned long long>(h->bundle_hash));
    return 0;
}

} // namespace asset
