#include "player_layout.hpp"

#include <utility>

#include "source_names.hpp"

namespace framework::input {

std::string shared_input_text(const ::input::SharedInput& shared) {
    std::string name = source_name(shared.src);
    if (name.empty()) name = "an unnamed input";
    std::string text = "player " + std::to_string(shared.player + 1) + " and player " +
                       std::to_string(shared.other + 1) + " share " + name;
    if (shared.pad_slot >= 0) text += " on pad " + std::to_string(shared.pad_slot + 1);
    return text;
}

bool build_layout(const PresetTable& table, uint32_t preset, const RebindStore& store,
                  ::input::ActionMap& map, int player, std::string& error) {
    error.clear();
    if (player < 0 || player >= ::input::MAX_PLAYERS) {
        error = "player index " + std::to_string(player) + " is out of range";
        return false;
    }
    ::input::ActionLayout layout;
    if (!table.bind(preset, layout)) {
        error = "preset " + std::to_string(preset) + " does not bind";
        return false;
    }
    store.apply(table, preset, layout);
    ::input::SharedInput shared;
    if (map.set_layout(player, std::move(layout), &shared)) return true;
    error = shared_input_text(shared);
    return false;
}

} // namespace framework::input
