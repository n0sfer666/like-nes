#pragma once
#include "save_slots.hpp"

namespace save {
// Game thread, before persistence use. Shell override wins (even explicitly empty), then
// nonempty absolute LIKENES_SAVE_DIR, otherwise user_data_dir(app). No exe/CWD fallback.
// A failed init/load/store reports once; later store retries the SAME intended root.
bool init(const std::string& app);
const std::string& dir();
// Compatibility files (ACH/controls keep their formats); validated canonical Backend key.
// Retries directory preparation after init failure, in the same intended root.
std::string file_path(const std::string& key);
// Explicit old absolute directory, known filenames supplied by the game. Durable exclusive move;
// existing destinations and unrelated files stay intact. Empty source means nothing to import.
bool migrate_legacy(const std::string& source_dir, std::span<const std::string> filenames);
// Global facade: requires init; quarantine returns an absolute path, not a backend key.
Status load(const char* slot, uint16_t version, std::span<const Migrate> chain,
            std::vector<uint8_t>& body, std::string* quarantine = nullptr);
bool store(const char* slot, uint16_t version, std::span<const uint8_t> body);
// Вызывающий сериализует запись одного пути; каталог должен уже существовать.
bool write_atomic(const std::string& path, std::span<const uint8_t> bytes);
} // namespace save
