#include "save_format.hpp"

#include <cstdio>
#include <limits>

#include "save_wire.hpp"

namespace {
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-format: FAIL %s\n", why); } }
using save::RecordResult;
}

int main() {
    // Независимый golden: struct.pack('<4sHHIQ',...) + zlib.crc32(header_without_crc + body).
    const std::vector<uint8_t> golden{0x4c, 0x4e, 0x53, 0x56, 0x01, 0x00, 0x02, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0xef, 0x12, 0xb0, 0x13, 0x10, 0x00, 0xff};
    const uint8_t body[]{16, 0, 255};
    std::vector<uint8_t> encoded;
    check(save::encode_record(2, 0x0102030405060708ull, body, encoded) && encoded == golden, "canonical LE bytes and IEEE CRC32");
    save::RecordView view;
    check(save::decode_record(encoded, view) == RecordResult::Ok && view.version == 2 &&
          view.writes == 0x0102030405060708ull && view.body.size() == 3 && view.body[2] == 255 &&
          view.body.data() == encoded.data() + 24, "zero-copy validated view");
    check(save::encode_record(view.version, view.writes, view.body, encoded) && encoded == golden, "encode from its own prior body");
    for (size_t i = 0; i < golden.size(); ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            auto bad = golden;
            bad[i] ^= static_cast<uint8_t>(1u << bit);
            check(save::decode_record(bad, view) == RecordResult::Corrupt && view.body.empty() &&
                  view.version == 0 && view.writes == 0, "every single-bit corruption rejected, including version and CRC");
        }
    }
    for (size_t size = 0; size < golden.size(); ++size)
        check(save::decode_record(std::span(golden).first(size), view) == RecordResult::Corrupt, "every truncation rejected");
    auto bad = golden;
    bad.push_back(0);
    check(save::decode_record(bad, view) == RecordResult::Corrupt, "trailing bytes rejected");
    bad = golden; bad[8] = 255; bad[9] = bad[10] = bad[11] = 255;
    save::detail::reseal_record(bad);
    check(save::decode_record(bad, view) == RecordResult::Corrupt, "resealed huge length rejected without allocation");
    bad = golden; bad[6] = 0; bad[7] = 0; save::detail::reseal_record(bad);
    check(save::decode_record(bad, view) == RecordResult::Corrupt, "zero slot version invalid even with valid CRC");
    bad = golden; bad[4] = 0; save::detail::reseal_record(bad);
    check(save::decode_record(bad, view) == RecordResult::Corrupt, "zero container version invalid");
    bad = golden; bad[4] = 2; save::detail::reseal_record(bad);
    check(save::decode_record(bad, view) == RecordResult::FutureContainer && view.body.size() == 3, "validated future container");
    bad[20] ^= 1;
    check(save::decode_record(bad, view) == RecordResult::Corrupt, "damaged future container is corrupt first");
    check(save::encode_record(65535, std::numeric_limits<uint64_t>::max(), {}, encoded) &&
          save::decode_record(encoded, view) == RecordResult::Ok && view.version == 65535 &&
          view.writes == std::numeric_limits<uint64_t>::max() && view.body.empty(), "full version/counter ranges and empty body");
    const auto before = encoded;
    check(!save::encode_record(0, 0, body, encoded) && encoded == before, "invalid encoder input keeps prior output");
    std::vector<uint8_t> huge(save::MAX_BODY_SIZE + 1);
    check(!save::encode_record(1, 1, huge, encoded) && encoded == before, "encoder body bound enforced");
    huge.resize(save::MAX_BODY_SIZE);
    check(save::encode_record(1, 1, huge, encoded) && save::decode_record(encoded, view) == RecordResult::Ok &&
          view.body.size() == save::MAX_BODY_SIZE, "maximum body size is accepted exactly");
    std::vector<uint8_t> unaligned(1, 0);
    unaligned.insert(unaligned.end(), golden.begin(), golden.end());
    check(save::decode_record(std::span(unaligned).subspan(1), view) == RecordResult::Ok && view.body[0] == 16, "unaligned wire buffer accepted");
    std::puts(fails ? "save-format: FAIL" : "save-format: PASS golden, CRC, bitflips, versions, bounds, unaligned and aliasing");
    return fails ? 1 : 0;
}
