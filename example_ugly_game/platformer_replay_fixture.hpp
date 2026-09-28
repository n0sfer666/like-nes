#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "net_wire.hpp"
#include "platform_fs.hpp"
#include "platformer_replay_io.hpp"

// Общая обстановка гейтов файла реплея: счётчик отказов, переписывание файла целиком и сборка
// заголовка. Заведена, когда перечисление порч переросло мягкий лимит длины и гейт разделили по
// предмету — доносит ли файл ТОТ ЖЕ прогон, тотален ли читатель по раскладке и что он отказывается
// ВЫДЕЛИТЬ по чужому заголовку (аудит #21, ревью A·2·3).
// Вторая копия сборки заголовка была бы второй правдой о его раскладке, а вторая копия `rewrite` —
// вторым мнением о том, что значит «переписать файл целиком».
namespace platformer::replay_fixture {

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

inline bool rewrite(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::FILE* f = platform::open_file(path, "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return std::fclose(f) == 0 && ok;
}

// Файл в один заголовок: гейты спрашивают разное у одной и той же двенадцатибайтовой подделки.
inline bool write_head(const std::string& file, uint32_t players, uint32_t ticks) {
    std::vector<uint8_t> head(replay_io::HEAD);
    net::Writer w(head.data(), head.size());
    w.bytes(replay_io::MAGIC, sizeof(replay_io::MAGIC));
    w.u32(players);
    w.u32(ticks);
    return w.ok() && rewrite(file, head);
}

} // namespace platformer::replay_fixture
