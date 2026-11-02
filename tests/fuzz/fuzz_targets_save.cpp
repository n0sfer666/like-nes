#include "fuzz_target.hpp"
#include "save_format.hpp"
#include "save_wire.hpp"

namespace fuzz {
namespace {
std::vector<uint8_t> seed_save() {
    std::vector<uint8_t> bytes;
    const uint8_t body[]{1, 0, 255, 12, 42};
    save::encode_record(1, 9, body, bytes);
    return bytes;
}
bool read_save(const uint8_t* data, size_t size) {
    save::RecordView view;
    const auto result = save::decode_record({data, size}, view);
    if (result == save::RecordResult::Corrupt) return false;
    consume_all(result, view.version, view.writes, view.body.size());
    for (uint8_t byte : view.body) consume(byte);
    return true;
}
void reseal_save(std::vector<uint8_t>& bytes) { save::detail::reseal_record(bytes); }
const Target TARGETS[]{{"save", seed_save, read_save, reseal_save}};
}
const Target* save_targets(size_t* count) {
    *count = sizeof TARGETS / sizeof TARGETS[0];
    return TARGETS;
}
} // namespace fuzz
