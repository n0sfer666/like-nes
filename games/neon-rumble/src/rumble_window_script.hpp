#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "input_engine.hpp"
#include "rumble_keys.hpp"

namespace rumble {

class WindowScript {
public:
    bool open(const std::string& path, std::string& why);
    void enter_level();
    void feed(bool level, uint32_t at, input::InputEngine& engine, WindowKeys& keys);
    const std::vector<std::string>& shots() const { return shots_; }
    bool quit() const { return quit_; }

private:
    enum class Verb : uint8_t { Down, Up, Shot, Quit };
    struct Step {
        uint32_t at;
        Verb verb;
        uint16_t key;
        std::string name;
    };
    struct Track {
        std::vector<Step> steps;
        size_t next = 0;
    };

    bool parse(const std::string& line, std::string& why);
    bool add_shot(Track& track, uint32_t at, const std::string& name, std::string& why);

    Track menu_;
    std::vector<Track> levels_;
    size_t entered_ = 0;
    size_t fed_ = 0;
    std::vector<uint16_t> held_;
    std::vector<std::string> names_;
    std::vector<std::string> shots_;
    bool quit_ = false;
};

} // namespace rumble
