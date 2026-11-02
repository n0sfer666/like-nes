#include "save_slots.hpp"
#include "save_test_backend.hpp"
#include "save_wire.hpp"

#include <array>
#include <cstdio>
#include <limits>

namespace {
using save::Status;
using save::test::record;
using save::test::counter;
int fails, migrated_calls;
save::test::Backend* active;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-slots: FAIL %s\n", why); } }
bool step2(std::vector<uint8_t>& body) {
    ++migrated_calls;
    check(active->files.contains("progress.v1.0.pre") && !active->files.contains("progress"), "backup precedes game callback");
    body.push_back(2); return true;
}
bool step3(std::vector<uint8_t>& body) { ++migrated_calls; body.push_back(3); return true; }
bool fail(std::vector<uint8_t>& body) { body.clear(); return false; }
bool oversize(std::vector<uint8_t>& body) { body.resize(save::MAX_BODY_SIZE + 1); return true; }
}

int main() {
    save::test::Backend backend;
    active = &backend;
    std::vector<uint8_t> body{99}; std::string quarantine = "stale";
    const std::array<save::Migrate, 2> chain{step2, step3};
    auto load = [&](uint16_t version = 1, std::span<const save::Migrate> steps = {}) {
        return save::load(backend, "progress", version, steps, body, &quarantine);
    };
    check(load() == Status::Empty && body.empty() && quarantine.empty(), "empty clears output");
    const std::vector<uint8_t> first{4, 5}, second{6};
    check(save::store(backend, "progress", 1, first) && counter(backend.files.at("progress")) == 1, "new slot counter1");
    check(save::store(backend, "progress", 1, second) && counter(backend.files.at("progress")) == 2, "write advances counter");
    const auto before_load = backend.files;
    check(load() == Status::Ok && body == second && backend.files == before_load, "roundtrip load does not rewrite");
    backend.files["neighbour"] = record(1, 999);
    const auto neighbour = backend.files.at("neighbour");
    const auto old = backend.files.at("progress");
    check(load(3, chain) == Status::Migrated && body == std::vector<uint8_t>({6, 2, 3}) &&
        quarantine == "progress.v1.0.pre" && migrated_calls == 2, "ordered v1->v3");
    check(backend.files.at(quarantine) == old && counter(backend.files.at("progress")) == 3, "original retained and migration counter advanced");
    backend.files["progress"] = record(2, 7);
    migrated_calls = 0;
    check(load(3, chain) == Status::Migrated && body == std::vector<uint8_t>({10, 3}) && migrated_calls == 1,
        "v2 starts at chain[1], not first callback");

    backend.files["progress"] = record(1, 20); backend.files["progress"][6] ^= 1;
    const auto corrupt = backend.files.at("progress");
    check(!save::store(backend, "progress", 1, first), "store cannot overwrite corrupt data before quarantine");
    check(load() == Status::Corrupt && body.empty() && quarantine == "progress.0.bad" &&
        backend.files.at(quarantine) == corrupt, "CRC checked before future, corrupt original preserved");
    check(save::store(backend, "progress", 1, first) && counter(backend.files.at("progress")) == 21,
        "counter continues from readable corrupt header");
    backend.files["progress"].back() ^= 1;
    check(load() == Status::Corrupt && quarantine == "progress.1.bad" && backend.files.at("progress.0.bad") == corrupt,
        "second corruption cannot overwrite prior backup");
    backend.files["progress"] = record(65535, 30);
    const auto future = backend.files.at("progress");
    backend.files["progress.v65535.0.bak"] = {99}; backend.files["progress.v65535.2.bak"] = {98};
    check(!save::store(backend, "progress", 1, first) && backend.files.at("progress") == future, "future cannot be overwritten");
    check(load() == Status::Future && quarantine == "progress.v65535.1.bak" && body.empty() &&
        backend.files.at(quarantine) == future, "future uses first free backup index");
    check(save::store(backend, "progress", 1, first) && counter(backend.files.at("progress")) == 31, "counter continues from future backup");
    backend.files["progress"] = record(1, 40); backend.files["progress"][4] = 2;
    save::detail::reseal_record(backend.files["progress"]);
    check(load() == Status::Future && quarantine == "progress.v1.0.bak", "validated future container quarantined");
    backend.files["progress"] = record(1, 50);
    const std::array<save::Migrate, 1> failing{fail}, huge{oversize};
    check(load(2, failing) == Status::MigrateFailed && body.empty() && !backend.files.contains("progress") &&
        counter(backend.files.at(quarantine)) == 50, "failed migration starts empty with original in pre");
    check(save::store(backend, "progress", 1, {}) && counter(backend.files.at("progress")) == 51, "empty body stored, no counter reset after failure");
    check(load(2, huge) == Status::MigrateFailed && body.empty(), "oversize migrated data refused, backup retained");

    backend.files["progress"] = record(1, 60);
    const auto stable = backend.files;
    backend.read_error = true;
    check(load() == Status::Error && body.empty() && quarantine.empty(), "IO failure not corruption");
    backend.read_error = false; backend.move_error = true;
    check(load(3, chain) == Status::Error && backend.files == stable && migrated_calls == 1, "move error preserves all data, no callback");
    backend.move_error = false; backend.list_error = true;
    check(load(3, chain) == Status::Error && backend.files == stable, "counter list failure precedes backup");
    check(!save::store(backend, "progress", 1, first) && backend.files == stable, "store fails closed on list error");
    backend.list_error = false; backend.write_error = true;
    check(load(2, {chain.data(), 1}) == Status::Error && body.empty() && !backend.files.contains("progress") &&
        counter(backend.files.at(quarantine)) == 60, "failed migrated publish retains original pre");
    backend.write_error = false;
    backend.files["progress"] = record(1, std::numeric_limits<uint64_t>::max());
    const auto max_record = backend.files.at("progress");
    check(!save::store(backend, "progress", 1, first) && backend.files.at("progress") == max_record, "counter cannot wrap");
    check(load(3, chain) == Status::Error && backend.files.at("progress") == max_record, "overflow refuses migration before move");
    backend.files["progress"].back() ^= 1;
    check(load() == Status::Corrupt && !save::store(backend, "progress", 1, first), "quarantine max readable counter still refuses wrap");
    check(backend.files.at("neighbour") == neighbour, "all failure paths preserve unrelated slot");

    save::test::Backend bounded;
    bounded.files["progress"] = record(1, 100);
    bounded.files["progress"].resize(save::RECORD_HEADER_SIZE + save::MAX_BODY_SIZE + 1);
    const auto giant = bounded.files.at("progress");
    check(save::load(bounded, "progress", 1, {}, body, &quarantine) == Status::Corrupt &&
        body.empty() && bounded.files.at(quarantine) == giant, "oversize physical record quarantined intact");
    bounded.read_error_key = quarantine;
    check(!save::store(bounded, "progress", 1, first) && !bounded.files.contains("progress"), "unreadable backup cannot reset counter");
    bounded.read_error_key.clear();
    check(save::store(bounded, "progress", 1, first) && counter(bounded.files.at("progress")) == 101,
        "oversize record readable header preserves counter");
    bounded.write_error = true;
    const auto write_before = bounded.files;
    check(!save::store(bounded, "progress", 1, second) && bounded.files == write_before, "failed store retains prior record");

    save::test::Backend isolated;
    isolated.files["progress.evil.bad"] = record(1, UINT64_MAX);
    isolated.files["progress.v0.0.pre"] = record(1, UINT64_MAX);
    isolated.files["progress.v1.01.pre"] = record(1, UINT64_MAX);
    isolated.files["progress2.0.bad"] = record(1, UINT64_MAX);
    check(save::store(isolated, "progress", 1, first) && counter(isolated.files.at("progress")) == 1,
        "only own canonical quarantine names affect counter");
    const auto invalid_before = isolated.files;
    const int calls = isolated.calls;
    const std::array<std::string, 7> invalid_slots{"", "../x", "progress.0.bad", "Progress", "con", "lpt1", std::string(64, 'a')};
    for (const auto& slot : invalid_slots) {
        check(save::load(isolated, slot, 1, {}, body) == Status::Invalid && !save::store(isolated, slot, 1, first), "invalid slot rejected");
    }
    const std::array<save::Migrate, 1> null{nullptr};
    check(save::load(isolated, "progress", 0, {}, body) == Status::Invalid &&
        save::load(isolated, "progress", 2, {}, body) == Status::Invalid &&
        save::load(isolated, "progress", 2, null, body) == Status::Invalid &&
        save::load(isolated, "progress", 1, chain, body) == Status::Invalid, "bad chain is programmer error");
    check(isolated.calls == calls && isolated.files == invalid_before, "invalid calls perform zero IO");
    check(!save::store(isolated, "progress", 2, first), "store cannot skip old-version migration");
    isolated.files["progress"] = {};
    check(save::load(isolated, "progress", 1, {}, body) == Status::Corrupt && body.empty(), "empty physical file is corrupt, not absent");
    std::puts(fails ? "save-slots: FAIL" : "save-slots: PASS roundtrip, migrations, quarantine, counters and failure preservation");
    return fails ? 1 : 0;
}
