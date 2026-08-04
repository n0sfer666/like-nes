#include "schedule.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"

// Гейт каркаса слоя: порядок систем восстанавливается из данных, а не из порядка регистрации.
// Проверяется именно расхождением — одно и то же множество регистрируется дважды в разных
// порядках, и последовательности обязаны совпасть посимвольно.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

std::vector<std::string>* g_trace = nullptr;

void record(void* user, const framework::Tick&) {
    if (g_trace != nullptr) g_trace->push_back(static_cast<const char*>(user));
}

using framework::BuildResult;
using framework::Stage;
using framework::SystemDesc;

SystemDesc sys(const char* name, Stage stage, const char* const* after = nullptr,
               std::size_t after_count = 0) {
    SystemDesc d;
    d.name = name;
    d.stage = stage;
    d.fn = &record;
    d.user = const_cast<char*>(name);
    d.after = after;
    d.after_count = after_count;
    return d;
}

std::string sequence(const framework::Schedule& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (!out.empty()) out += ' ';
        out += s.name_at(i);
    }
    return out;
}

const char* AFTER_AIM[] = {"aim"};
const char* AFTER_MOVE[] = {"move"};
const char* AFTER_RESOLVE[] = {"resolve"};
const char* AFTER_CAMERA[] = {"camera"};

// Пять систем в двух стадиях: «move» обязана идти после «aim» вопреки алфавиту, значит порядок
// доказывается зависимостями, а не сортировкой имён целиком.
void fill_forward(framework::Schedule& s) {
    s.add(sys("resolve", Stage::Input));
    s.add(sys("aim", Stage::PreSim, AFTER_RESOLVE, 1));
    s.add(sys("move", Stage::PreSim, AFTER_AIM, 1));
    s.add(sys("bounds", Stage::PreSim));
    s.add(sys("camera", Stage::PostSim, AFTER_MOVE, 1));
}

void fill_reverse(framework::Schedule& s) {
    s.add(sys("camera", Stage::PostSim, AFTER_MOVE, 1));
    s.add(sys("bounds", Stage::PreSim));
    s.add(sys("move", Stage::PreSim, AFTER_AIM, 1));
    s.add(sys("aim", Stage::PreSim, AFTER_RESOLVE, 1));
    s.add(sys("resolve", Stage::Input));
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);

    framework::Schedule a;
    fill_forward(a);
    check(a.build() == BuildResult::Ok, "a well-formed schedule builds");
    const std::string seq_a = sequence(a);

    framework::Schedule b;
    fill_reverse(b);
    check(b.build() == BuildResult::Ok, "the same systems build in the reverse registration order");
    const std::string seq_b = sequence(b);

    std::printf("  order: %s\n", seq_a.c_str());
    check(seq_a == seq_b, "registration order does not change execution order");
    check(seq_a == "resolve aim bounds move camera", "stages, dependencies and name tie-break hold");

    // Исполнение обязано идти тем же порядком, что и разложенная последовательность: расписание,
    // печатающее один порядок и исполняющее другой, прошло бы проверку выше целиком.
    std::vector<std::string> trace;
    g_trace = &trace;
    a.run({7, fix32::from_int(1)});
    std::string ran;
    for (const std::string& n : trace) {
        if (!ran.empty()) ran += ' ';
        ran += n;
    }
    check(ran == seq_a, "run() executes exactly the order it reports");

    trace.clear();
    a.run_stage(Stage::PreSim, {7, fix32::from_int(1)});
    ran.clear();
    for (const std::string& n : trace) {
        if (!ran.empty()) ran += ' ';
        ran += n;
    }
    check(ran == "aim bounds move", "run_stage() runs its own stage and nothing else");
    g_trace = nullptr;

    framework::Schedule dup;
    check(dup.add(sys("move", Stage::Sim)), "the first registration is accepted");
    check(!dup.add(sys("move", Stage::Sim)), "a duplicate name is refused at registration");

    framework::Schedule missing;
    missing.add(sys("move", Stage::Sim, AFTER_AIM, 1));
    check(missing.build() == BuildResult::UnknownDependency, "an unknown dependency is named");
    check(missing.error_system() == "move", "the failing system is named, not just the reason");

    // Зависимость из более поздней стадии неудовлетворима по построению сетки, и молчаливое
    // «стадии всё равно упорядочены» скрыло бы опечатку в стадии на годы.
    framework::Schedule backwards;
    backwards.add(sys("camera", Stage::PostSim));
    backwards.add(sys("move", Stage::Sim, AFTER_CAMERA, 1));
    check(backwards.build() == BuildResult::LaterStageDependency,
          "a dependency on a later stage is refused");

    framework::Schedule cyclic;
    cyclic.add(sys("aim", Stage::Sim, AFTER_MOVE, 1));
    cyclic.add(sys("move", Stage::Sim, AFTER_AIM, 1));
    check(cyclic.build() == BuildResult::Cycle, "a cycle inside a stage is reported, not hung");

    framework::Schedule unbuilt;
    unbuilt.add(sys("move", Stage::Sim, AFTER_AIM, 1));
    unbuilt.build();
    trace.clear();
    g_trace = &trace;
    unbuilt.run({0, fix32{}});
    g_trace = nullptr;
    check(trace.empty(), "a schedule that failed to build runs nothing");

    const bool pass = (fails == 0);
    std::printf("framework-schedule: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
