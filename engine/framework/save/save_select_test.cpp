#include "save_backend.hpp"
#include "save_format.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>

namespace {
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-select: FAIL %s\n", why); } }
class FakeBackend final : public save::Backend {
public:
    std::map<std::string, std::vector<uint8_t>> files;
    save::ReadResult read(const std::string& key, std::vector<uint8_t>& out, size_t limit) override {
        out.clear();
        const auto found = files.find(key);
        if (found == files.end()) return save::ReadResult::Missing;
        const auto& bytes = found->second;
        out.assign(bytes.begin(), bytes.begin() + std::min(bytes.size(), limit));
        return bytes.size() > limit ? save::ReadResult::TooLarge : save::ReadResult::Ok;
    }
    bool write(const std::string& key, std::span<const uint8_t> bytes) override {
        files[key] = std::vector<uint8_t>(bytes.begin(), bytes.end()); return true;
    }
    bool list(std::vector<std::string>& out) override {
        out.clear(); for (const auto& [key, bytes] : files) { (void)bytes; out.push_back(key); } return true;
    }
    save::MoveResult move_new(const std::string& from, const std::string& to) override {
        if (files.contains(to)) return save::MoveResult::Exists;
        const auto source = files.find(from);
        if (source == files.end()) return save::MoveResult::Error;
        files[to] = std::move(source->second); files.erase(source); return save::MoveResult::Moved;
    }
};

std::vector<uint8_t> record(uint16_t version, uint64_t writes, uint8_t body) {
    std::vector<uint8_t> bytes;
    check(save::encode_record(version, writes, {&body, 1}, bytes), "fixture encoded");
    return bytes;
}
}

int main() {
    using save::SelectionResult;
    FakeBackend local, peer;
    const auto older = record(1, 4, 9), newer = record(1, 9, 3);
    check(local.write("progress", older) && peer.write("progress", newer), "two independent fake backends");
    std::array<std::vector<uint8_t>, 2> copies;
    check(local.read("progress", copies[0], 1024) == save::ReadResult::Ok &&
          peer.read("progress", copies[1], 1024) == save::ReadResult::Ok, "both copies read through backend");
    auto selected = save::select_record(copies);
    check(selected.result == SelectionResult::Selected && selected.index == 1 && selected.writes == 9, "larger counter wins, no clock input");
    std::swap(copies[0], copies[1]); selected = save::select_record(copies);
    check(selected.index == 0 && selected.writes == 9, "winner independent of replica order");
    copies[1] = record(1, 9, 8);
    const auto before = copies;
    selected = save::select_record(copies);
    check(selected.result == SelectionResult::Conflict && selected.index == static_cast<size_t>(-1) &&
          selected.writes == 9 && copies == before, "equal counters preserve both divergent copies");
    copies[1] = copies[0]; selected = save::select_record(copies);
    check(selected.result == SelectionResult::Selected && selected.index == 0, "identical replicas are not a conflict");
    copies[1] = record(65535, 9, 3);
    check(save::select_record(copies).result == SelectionResult::Conflict, "different slot versions are distinct data");
    std::vector<std::vector<uint8_t>> three{record(1, 4, 1), record(1, 4, 2), record(1, 5, 3)};
    selected = save::select_record(three);
    check(selected.result == SelectionResult::Selected && selected.index == 2, "lower-counter conflict does not shadow newer copy");
    copies[0] = newer; copies[1] = record(1, 999, 1); copies[1][24] ^= 1;
    selected = save::select_record(copies);
    check(selected.result == SelectionResult::Selected && selected.index == 0, "corrupt high counter cannot win");
    copies[0].clear();
    check(save::select_record(copies).result == SelectionResult::Corrupt, "all bad copies reported");
    check(save::select_record({}).result == SelectionResult::Empty, "no copies is empty");
    check(local.files.at("progress") == older && peer.files.at("progress") == newer, "selection never overwrites either backend");
    std::puts(fails ? "save-select: FAIL" : "save-select: PASS two backends, monotonic choice, conflict, corruption and preservation");
    return fails ? 1 : 0;
}
