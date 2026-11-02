#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "framework_scene_level_fixture.hpp"

namespace {

namespace sc = framework::scene;
using namespace level_fixture;
using Inputs = std::vector<asset::AssetInput>;

int fails = 0;
int cases = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        ++fails;
    }
}

struct Bad {
    const char* what;
    std::function<void(Source&)> edit_source;
    std::function<void(Inputs&)> edit_bundle;
    const char* name;
    rumble::StageFault want;
    const char* text;
};

void drop(Inputs& in, uint64_t guid) {
    std::erase_if(in, [&](const asset::AssetInput& a) { return a.guid == guid; });
}

void corrupt(Inputs& in, const char* table) {
    asset::AssetInput* a = find(in, table);
    if (a != nullptr) a->payload[0] ^= 0xFF;
}

void truncate(Inputs& in, const char* table) {
    asset::AssetInput* a = find(in, table);
    if (a == nullptr) return;
    a->payload.resize(a->payload.size() / 2);
    a->uncompressed_size = static_cast<uint32_t>(a->payload.size());
}

tl::ObjectSrc& object(Source& s, uint32_t id) {
    for (tl::ObjectSrc& o : s.objects[0].objects)
        if (o.id == id) return o;
    return s.objects[0].objects[0];
}

void kind(Source& s, uint32_t id, const char* to) {
    for (tl::ObjectPropSrc& p : object(s, id).props)
        if (p.name == "kind") p.text = to;
}

void order(Source& s, int32_t to) {
    for (tl::ObjectPropSrc& p : object(s, 14).props)
        if (p.name == "order") p.value = to;
}

void second_checkpoint(Source& s, int32_t to) {
    s.objects[0].objects.push_back(scene_fixture::with(scene_fixture::point(40, "cp9", "checkpoint", 600, 250),
                                                       {scene_fixture::int_prop("order", to)}));
}

const std::string LONG_NAME(300, 'x');

const Bad CASES[] = {
    {"absent name", {}, {}, "nope", rumble::StageFault::NoLevel, "no level nope in the visual table"},
    {"prefix of a name", {}, {}, "lv", rumble::StageFault::NoLevel, "no level lv in the visual table"},
    {"empty name", {}, {}, "", rumble::StageFault::NoLevel, "in the visual table"},
    {"null name", {}, {}, nullptr, rumble::StageFault::NoLevel, "level name is null"},
    {"long name is cut, not overrun", {}, {}, LONG_NAME.c_str(), rumble::StageFault::NoLevel, "no level xxx"},
    {"level absent from LNTM", [](Source& s) { s.maps[0].name = "gone"; }, {}, "lvl", rumble::StageFault::NoLevel,
     "no level lvl in the tilemap table"},
    {"level absent from LNOB", [](Source& s) { s.objects[0].name = "gone"; }, {}, "lvl", rumble::StageFault::NoLevel,
     "no level lvl in the objects table"},
    {"no LNTM", {}, [](Inputs& in) { drop(in, asset::fnv1a_str("tilemap")); }, "lvl", rumble::StageFault::NoTable,
     "tilemap table: missing"},
    {"no LNVL", {}, [](Inputs& in) { drop(in, asset::fnv1a_str("visual")); }, "lvl", rumble::StageFault::NoTable,
     "visual table: missing"},
    {"no LNOB", {}, [](Inputs& in) { drop(in, asset::fnv1a_str("objects")); }, "lvl", rumble::StageFault::NoTable,
     "objects table: missing"},
    {"no LNCL", {}, [](Inputs& in) { drop(in, asset::fnv1a_str("clips")); }, "lvl", rumble::StageFault::NoTable,
     "clips table: missing"},
    {"texture under the LNOB name", {},
     [](Inputs& in) {
         drop(in, asset::fnv1a_str("objects"));
         asset::AssetInput t = texture(asset::fnv1a_str("objects"), 1, 1);
         in.push_back(t);
     },
     "lvl", rumble::StageFault::NoTable, "objects table: wrong type"},
    {"broken LNOB magic", {}, [](Inputs& in) { corrupt(in, "objects"); }, "lvl", rumble::StageFault::BadTable,
     "objects table does not open"},
    {"truncated LNOB", {}, [](Inputs& in) { truncate(in, "objects"); }, "lvl", rumble::StageFault::BadTable,
     "objects table does not open"},
    {"broken LNVL", {}, [](Inputs& in) { corrupt(in, "visual"); }, "lvl", rumble::StageFault::BadTable,
     "visual table does not open"},
    {"broken LNTM", {}, [](Inputs& in) { corrupt(in, "tilemap"); }, "lvl", rumble::StageFault::BadTable,
     "tilemap table does not open"},
    {"broken LNCL", {}, [](Inputs& in) { corrupt(in, "clips"); }, "lvl", rumble::StageFault::BadTable,
     "clips table does not open"},
    {"LNOB refused before a missing texture", [](Source& s) { kind(s, 7, ""); }, [](Inputs& in) { drop(in, STREET); }, "lvl", rumble::StageFault::BadObjects,
     "level lvl: "},
    {"checkpoint order past int16", [](Source& s) { order(s, 32768); }, {}, "lvl", rumble::StageFault::BadObjects,
     "level lvl: checkpoint 'cp1' (id 14) property 'order' is 32768; allowed 0 to 32767"},
    {"checkpoint order repeated", [](Source& s) { second_checkpoint(s, 1); }, {}, "lvl", rumble::StageFault::BadObjects,
     "level lvl: checkpoint 'cp9' (id 40) repeats checkpoint order 1 of object id 14"},
    {"unknown kind", [](Source& s) { kind(s, 7, "ghost"); }, {}, "lvl", rumble::StageFault::UnknownKind,
     "level lvl: spawn grunt1 kind ghost: no table ghost_fighter"},
    {"kind of the last spawn has no LNFT", {}, [](Inputs& in) { drop(in, asset::fnv1a_str("banderas_fighter")); },
     "lvl", rumble::StageFault::UnknownKind, "spawn boss kind banderas: no table banderas_fighter"},
    {"broken LNFT", {}, [](Inputs& in) { corrupt(in, "adler_fighter"); }, "lvl", rumble::StageFault::BadFighter,
     "adler_fighter table does not open"},
    {"LNFT under a texture", {},
     [](Inputs& in) {
         drop(in, asset::fnv1a_str("adler_fighter"));
         in.push_back(texture(asset::fnv1a_str("adler_fighter"), 1, 1));
     },
     "lvl", rumble::StageFault::BadFighter, "adler_fighter table: wrong type"},
    {"LNFT move without its clip in LNCL",
     [](Source& s) { std::erase_if(s.clips, [](const gr::ClipSrc& c) { return c.name == "adler/kick"; }); }, {},
     "lvl", rumble::StageFault::BadFighter, "adler_fighter: no clip adler/kick in the clips table"},
    {"nine textures", [](Source& s) { add_images(s, 7); }, {}, "lvl", rumble::StageFault::Capacity,
     "level lvl needs more than 8 textures"},
    {"nine fighter kinds", [](Source& s) { add_kinds(s, 7); }, {}, "lvl", rumble::StageFault::Capacity,
     "level lvl needs more than 8 fighter kinds"},
    {"image layer texture missing", {}, [](Inputs& in) { drop(in, SKY); }, "lvl", rumble::StageFault::Texture,
     ": missing"},
    {"tileset texture of a foreign format", {},
     [](Inputs& in) {
         for (asset::AssetInput& a : in)
             if (a.guid == STREET) a.tex_format = asset::TEX_FORMAT_RGBA8_UNORM + 1;
     },
     "lvl", rumble::StageFault::Texture, "wrong texture format"},
};

bool reset(const rumble::Stage& l) {
    return l.level.name[0] == '\0' && !l.level.collision && l.level.map.row == nullptr && l.level.object_map.row == nullptr &&
           l.objects.waves.empty() && l.level.texture_count == 0 && l.fighter_count == 0 && !l.level.clips.valid();
}

bool loaded(rumble::Stage& l) {
    Inputs in;
    std::string error;
    asset::BundleView bundle;
    rumble::StageError e;
    return bake(source(), in, error) && open(std::move(in), bundle) && rumble::load_stage(bundle, "lvl", l, e);
}

void run(const Bad& c) {
    ++cases;
    const std::string what = c.what;
    Source s = source();
    if (c.edit_source) c.edit_source(s);
    Inputs in;
    std::string error;
    if (!bake(s, in, error)) return check(false, what + ": fixture does not bake: " + error);
    if (c.edit_bundle) c.edit_bundle(in);
    rumble::Stage l;
    check(loaded(l), what + ": the Level is loaded before the refusal");
    asset::BundleView bundle;
    if (!open(std::move(in), bundle)) return check(false, what + ": bundle does not open");
    rumble::StageError e;
    check(!rumble::load_stage(bundle, c.name, l, e), what + ": refused");
    check(e.fault == c.want, what + ": fault " + rumble::stage_fault_name(e.fault) + ", want " +
                                 rumble::stage_fault_name(c.want));
    check(std::strstr(e.text, c.text) != nullptr, what + ": text '" + e.text + "' lacks '" + c.text + "'");
    check(std::strlen(e.text) < sc::LOAD_ERROR_TEXT, what + ": text fits its buffer");
    check(reset(l), what + ": the Level is cleared, not left half loaded");
}

void test_cut() {
    ++cases;
    rumble::StageError e;
    rumble::stage_fail(e, rumble::StageFault::NoLevel, std::string(1000, 'y'));
    check(std::strlen(e.text) == sc::LOAD_ERROR_TEXT - 1 && e.text[0] == 'y', "long text is cut at the buffer");
}

} // namespace

int main() {
    std::printf("level load refuses broken bundles with a reason\n");
    rumble::Stage base;
    check(loaded(base), "base bundle loads before any edit");
    for (const Bad& c : CASES) run(c);
    test_cut();
    std::printf("framework-scene-level-refusal: %s - %d cases\n", fails == 0 ? "PASS" : "FAIL", cases);
    return fails == 0 ? 0 : 1;
}
