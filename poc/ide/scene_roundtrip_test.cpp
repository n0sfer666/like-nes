#include "scene.hpp"
#include "serialize.hpp"
#include <cstdint>
#include <cstdio>
#include <string>

// Гейт 1 (спека #7): save->reload->save байт-идентичны + детерм. golden bake-hash + run-to-run.
// fix32 целочисл. → golden cross-arch стабилен (как asset/audio/input/plugin golden).
using namespace ide;

namespace {

// Детерм. билд представит. сцены. Порядок создания (10,20,5) != порядок GUID → проверяет
// сортировку сериализации по GUID. fix32 из int/raw/float-границы (авторинг).
void build_scene(Scene& s) {
    auto e10 = s.create(10);
    e10.set<Name>({"hero"});
    e10.set<Position>({fix32::from_int(3), fix32::from_int(5)});
    e10.set<Velocity>({fix32::from_raw(1234), fix32::from_raw(-5678)});

    auto e20 = s.create(20);
    e20.set<Name>({"sword"});
    e20.set<Parent>({10});
    e20.set<Position>({fix32::from_int(1), fix32::from_int(0)});

    auto e5 = s.create(5);
    e5.set<Name>({"camera"});
    e5.set<Position>({fix32::from_float(-2.5), fix32::from_float(7.25)});
    e5.set<Velocity>({fix32(), fix32()});
}

} // namespace

// Golden bake-hash (FNV-1a над канонич. текстом). Обновляется при изменении build_scene/формата.
static constexpr uint64_t GOLDEN = 0x2de54a36e54e0684ull;

int main() {
    Scene a;
    build_scene(a);
    std::string s1 = serialize(a);

    Scene b;
    deserialize(b, s1);
    std::string s2 = serialize(b);
    bool roundtrip = (s1 == s2);

    Scene c;
    build_scene(c);
    bool run2run = (serialize(c) == s1);

    uint64_t golden = golden_hash(a);
    std::printf("scene golden-hash: 0x%016llx\n", (unsigned long long)golden);
    std::printf("round-trip byte-identical: %s\n", roundtrip ? "YES" : "NO");
    std::printf("run-to-run identical: %s\n", run2run ? "YES" : "NO");

    if (!roundtrip) {
        std::fprintf(stderr, "--- s1 ---\n%s--- s2 ---\n%s", s1.c_str(), s2.c_str());
    }

    bool golden_ok = (GOLDEN == 0x0ull) || (golden == GOLDEN);
    bool pass = roundtrip && run2run && golden_ok;
    std::printf("scene-roundtrip: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
