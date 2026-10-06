#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include "framework_brawl_check.hpp"
#include "framework_brawl_crossplay.hpp"
#include "framework_brawl_scenarios.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"

namespace {

using namespace framework::brawl;
using test::check;

const uint64_t PINS[] = {
    0x6355ad5af1533844ull,
    0xca7b73affdccacb3ull,
    0xa7087e032b245e83ull,
    0x245cfd4c72637269ull,
    0xdecacb80b3ba1d83ull,
    0xf0e83c5483b4d24dull,
    0xd018613f733fbb08ull,
    0x0c2b7fdfafd5e501ull,
};

constexpr uint32_t SCENARIO_COUNT = sizeof(scenario::SCENARIOS) / sizeof(scenario::SCENARIOS[0]);
static_assert(SCENARIO_COUNT == CROSSPLAY_SCENARIOS, "a scenario was added: raise CROSSPLAY_SCENARIOS");
static_assert(sizeof(PINS) / sizeof(PINS[0]) == SCENARIO_COUNT, "every scenario has its pin");

bool write_hashes(const std::string& path, const uint64_t* hashes) {
    std::FILE* f = platform::open_file(path, "wb");
    if (f == nullptr) return false;
    bool ok = true;
    for (uint32_t i = 0; i < SCENARIO_COUNT; ++i)
        ok = std::fprintf(f, "%s 0x%016" PRIx64 "\n", scenario::SCENARIOS[i].name, hashes[i]) > 0 && ok;
    return std::fclose(f) == 0 && ok;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string out;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--crossplay") != 0) continue;
        if (i + 1 == argc) {
            std::fprintf(stderr, "framework-brawl-crossplay: --crossplay needs a file path\n");
            return 2;
        }
        out = argv[++i];
    }

    std::printf("brawl: scripted depth runs, hashed every tick\n");
    uint64_t hashes[SCENARIO_COUNT];
    for (uint32_t i = 0; i < SCENARIO_COUNT; ++i) {
        const scenario::Scenario& s = scenario::SCENARIOS[i];
        uint32_t hits = 0;
        hashes[i] = scenario::run(s, &hits);
        const bool same = hashes[i] == scenario::run(s);
        std::printf("  %s 0x%016" PRIx64 "\n", s.name, hashes[i]);
        check(same, "a scenario run twice in one process gives one hash");
        check(hashes[i] == PINS[i], "the scenario hash matches its pinned literal");
        check((s.moves == scenario::no_moves) == (hits == 0), "a scenario lands hits exactly when it strikes");
    }
    if (!out.empty()) check(write_hashes(out, hashes), "the crossplay file is written");
    return test::verdict("framework-brawl-crossplay");
}
