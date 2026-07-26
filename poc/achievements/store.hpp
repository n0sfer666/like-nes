#pragma once
#include "state.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ach {

class Tracker;

std::string temp_path_for(const std::string& path);
bool write_atomic(const std::string& path, const uint8_t* data, std::size_t size);
bool read_file(const std::string& path, std::vector<uint8_t>& out,
               std::size_t max_bytes = STATE_MAX_SIZE);

class LocalStore {
public:
    explicit LocalStore(std::string path) : path_(std::move(path)) {}

    bool load(Tracker& tr, DecodeResult* why = nullptr) const;
    bool save(const Tracker& tr) const;

    const std::string& path() const { return path_; }
    std::string temp_path() const { return temp_path_for(path_); }

private:
    std::string path_;
};

} // namespace ach
