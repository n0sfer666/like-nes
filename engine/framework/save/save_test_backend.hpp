#pragma once
// Test-only in-memory IO fault injection; not installed in the SDK.
#include "save_backend.hpp"
#include "save_format.hpp"

#include <algorithm>
#include <map>

namespace save::test {
struct Backend final : save::Backend {
    std::map<std::string, std::vector<uint8_t>> files;
    bool read_error = false, write_error = false, list_error = false, move_error = false;
    std::string read_error_key;
    int calls = 0;
    ReadResult read(const std::string& key, std::vector<uint8_t>& out, size_t limit) override {
        ++calls; out.clear();
        if (read_error || key == read_error_key) return ReadResult::Error;
        const auto found = files.find(key);
        if (found == files.end()) return ReadResult::Missing;
        const auto& bytes = found->second;
        out.assign(bytes.begin(), bytes.begin() + std::min(limit, bytes.size()));
        return bytes.size() > limit ? ReadResult::TooLarge : ReadResult::Ok;
    }
    bool write(const std::string& key, std::span<const uint8_t> bytes) override {
        ++calls;
        if (write_error) return false;
        files[key] = std::vector<uint8_t>(bytes.begin(), bytes.end()); return true;
    }
    bool list(std::vector<std::string>& keys) override {
        ++calls; keys.clear();
        if (list_error) return false;
        for (const auto& [key, bytes] : files) { (void)bytes; keys.push_back(key); }
        return true;
    }
    MoveResult move_new(const std::string& from, const std::string& to) override {
        ++calls;
        if (move_error) return MoveResult::Error;
        if (files.contains(to)) return MoveResult::Exists;
        const auto found = files.find(from);
        if (found == files.end()) return MoveResult::Error;
        files[to] = std::move(found->second); files.erase(found); return MoveResult::Moved;
    }
};
inline std::vector<uint8_t> record(uint16_t version, uint64_t writes, std::vector<uint8_t> body = {10}) {
    std::vector<uint8_t> bytes;
    encode_record(version, writes, body, bytes);
    return bytes;
}
inline uint64_t counter(const std::vector<uint8_t>& bytes) {
    RecordView view;
    return decode_record(bytes, view) == RecordResult::Ok ? view.writes : 0;
}
}
