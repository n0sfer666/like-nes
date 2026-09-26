#include <cstdio>
#include <string>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "world.hpp"

// Аудит #21 B10: дескриптор тела, которого в мире нет, — `INVALID` от `add` с занятым ключом или
// индекс за последним телом — обязан ронять процесс со строкой в stderr, а не читать мимо массива.
// Падение проверяется подпроцессом: тест перезапускает собственный exe со служебным режимом. Режим
// `valid` — позитивный контроль той же машинерии: без него «ребёнок упал» доказывал бы только то,
// что ребёнок не запустился. Цель только Release: в Debug MSVC `abort` открывает модальное окно.
namespace {

using namespace framework;
using namespace framework::physics;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

BodyDesc desc(uint32_t key) {
    BodyDesc d;
    d.key = key;
    d.shape = box(fix32::from_int(1), fix32::from_int(1));
    return d;
}

int child(const std::string& mode) {
    World w(4);
    const BodyId first = w.add(desc(1));
    const BodyId refused = w.add(desc(1));
    if (mode == "valid") return w.body(first).key == 1 && w.mutate(first).key == 1 ? 0 : 2;
    if (mode == "refused") return static_cast<int>(w.mutate(refused).key);
    if (mode == "past-end") return static_cast<int>(w.body(BodyId{1}).key);
    return 3;
}

void expect_death(const std::string& self, const char* mode, const char* line, const char* what) {
    std::string out;
    platform::ExitStatus st;
    const bool ran = platform::run_capture({self, mode}, out, st);
    const bool died = st.kind == platform::ExitKind::Crashed ||
                      (st.kind == platform::ExitKind::Exited && st.code != 0);
    if (!ran || !died || out.find(line) == std::string::npos)
        std::printf("  (%s: kind=%d code=%d output=\"%s\")\n", mode, static_cast<int>(st.kind), st.code,
                    out.c_str());
    check(ran && died, what);
    check(out.find(line) != std::string::npos, "and names the reason on stderr");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc > 1) return child(argv[1]);
    std::printf("framework physics id gate\n");
    World w(4);
    check(w.add(desc(1)).valid() && !w.add(desc(1)).valid(), "add() with a taken key returns INVALID");
    const std::string self = platform::exe_path();
    check(!self.empty(), "own exe path resolved");
    std::string out;
    platform::ExitStatus st;
    const bool ran = platform::run_capture({self, "valid"}, out, st);
    check(ran && st.kind == platform::ExitKind::Exited && st.code == 0, "a valid id reads and writes its own body");
    expect_death(self, "refused", "[physics] body id is INVALID", "mutate() on a refused add() aborts");
    expect_death(self, "past-end", "[physics] body id 1 is past the last body (1 bodies)",
                 "body() past the last body aborts");
    std::printf("framework-physics-id: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
