#include "save_slots.hpp"

#include <utility>

#include "save_format.hpp"
#include "save_slots_internal.hpp"

namespace save {
namespace {
constexpr size_t MAX_RECORD_SIZE = RECORD_HEADER_SIZE + MAX_BODY_SIZE;
Status isolate(Backend& backend, const std::string& slot, const std::string& tag,
               const std::string& extension, Status status, std::string* out) {
    std::string key;
    if (!detail::quarantine(backend, slot, tag, extension, key)) return Status::Error;
    if (out) *out = std::move(key);
    return status;
}
}

Status load(Backend& backend, const std::string& slot, uint16_t version, std::span<const Migrate> chain,
            std::vector<uint8_t>& body, std::string* quarantine) {
    body.clear();
    if (quarantine) quarantine->clear();
    if (!detail::valid_load(slot, version, chain)) return Status::Invalid;
    std::vector<uint8_t> bytes;
    const auto read = backend.read(slot, bytes, MAX_RECORD_SIZE);
    if (read == ReadResult::Missing) return Status::Empty;
    if (read == ReadResult::Error) return Status::Error;
    RecordView record;
    const auto decoded = read == ReadResult::TooLarge ? RecordResult::Corrupt : decode_record(bytes, record);
    if (decoded == RecordResult::Corrupt)
        return isolate(backend, slot, "", ".bad", Status::Corrupt, quarantine);
    const std::string tag = ".v" + std::to_string(record.version);
    if (decoded == RecordResult::FutureContainer || record.version > version)
        return isolate(backend, slot, tag, ".bak", Status::Future, quarantine);
    if (record.version == version) {
        body.assign(record.body.begin(), record.body.end());
        return Status::Ok;
    }
    uint64_t next;
    if (!detail::next_counter(backend, slot, next)) return Status::Error;
    const auto isolated = isolate(backend, slot, tag, ".pre", Status::Migrated, quarantine);
    if (isolated != Status::Migrated) return isolated;
    std::vector<uint8_t> migrated(record.body.begin(), record.body.end());
    for (size_t i = record.version - 1; i < chain.size(); ++i)
        if (!chain[i](migrated) || migrated.size() > MAX_BODY_SIZE) return Status::MigrateFailed;
    std::vector<uint8_t> encoded;
    if (!encode_record(version, next, migrated, encoded) || !backend.write(slot, encoded)) return Status::Error;
    body.swap(migrated);
    return Status::Migrated;
}

bool store(Backend& backend, const std::string& slot, uint16_t version, std::span<const uint8_t> body) {
    if (!detail::valid_slot(slot) || version == 0 || body.size() > MAX_BODY_SIZE) return false;
    std::vector<uint8_t> bytes;
    const auto read = backend.read(slot, bytes, MAX_RECORD_SIZE);
    if (read != ReadResult::Missing) {
        RecordView record;
        if (read != ReadResult::Ok || decode_record(bytes, record) != RecordResult::Ok || record.version != version) return false;
    }
    uint64_t next;
    if (!detail::next_counter(backend, slot, next) || !encode_record(version, next, body, bytes)) return false;
    return backend.write(slot, bytes);
}
} // namespace save
