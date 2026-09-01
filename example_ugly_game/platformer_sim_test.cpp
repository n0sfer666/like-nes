#include <cstdio>
#include <cstring>
#include <string>

#include "platform_args.hpp"
#include "platformer_sim.hpp"

// Гейт образца-платформера (шаг C вертикали 3 спеки #16), машинная половина гейта 8: скриптованный
// прогон по уровню из БАНДЛА даёт побитово тот же хеш траектории на macOS, Linux и Windows.
//
// Голден прибит ЗДЕСЬ числом, а не грепом в шаге CI: та же цель гоняется Debug-этапом, где вопрос
// не про предупреждения, а про совпадение `-O0` и `-O3`, — и число, живущее в workflow, до этого
// этапа не доезжает вовсе.
//
// Хеша ОДНОГО мало, и это не перестраховка. Хеш отвечает «три машины сошлись», и маршрут, ни разу
// не задевший склон, отвечает так же уверенно, как задевший, — ровно этим кончился B1b, где голден
// траектории молчал про склоны, потому что в его сцене их не было. Поэтому прогон ещё и
// ОТЧИТЫВАЕТСЯ, что он потрогал, и каждый приём стоит здесь отдельным утверждением.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

// Голден маршрута. Двигается ТОЛЬКО вместе с осознанной правкой карты, профиля, скрипта или самого
// контроллера — и перепечатывается тем же прогоном, который его печатает при расхождении.
constexpr uint64_t GOLDEN_HASH = 0xfead7a87477a9258ull;

// Находка владельческого прогона §6 от 2026-09-01: «пошёл налево за экран — герой пропал, но
// управление осталось». За левым краем сетки нет ни пола, ни стены, и персонаж уходил в пустоту
// живым и управляемым, то есть игра продолжалась ВНЕ уровня.
//
// Утверждение здесь, а не в скрипте прогона: голден маршрута отвечает «три ОС сошлись» и про край
// уровня не говорит ничего — маршрут его не задевает вовсе. Дописать край в скрипт значило бы
// сдвинуть голден ради случая, который к нему отношения не имеет.
void test_the_level_has_edges(const std::string& path) {
    platformer::Stage st;
    if (!platformer::load_stage(path, st)) {
        check(false, "the level loads for the edge run");
        return;
    }
    const fix32 left = st.grid->origin().x + platformer::HULL_HALF_W;
    // Влево С ПРЫЖКОМ, а не просто влево: стена в левом краю карты высотой в три тайла (48), а
    // прыжок берёт 64, и персонаж перелетает её ПОВЕРХ. Ходьба в стену упиралась бы в неё на 24.125
    // и проходила бы это утверждение, ни разу не подойдя к краю уровня, — то есть гейт был бы зелен
    // вакуумно. Владелец так и вышел: пошёл налево и нажал прыжок.
    platformer::ch::MoveInput in;
    in.move_x = fix32::from_int(-1);
    // Самое левое положение за прогон, а не только конечное: кламп ставит персонажа на край, а
    // тайл края его оттуда выталкивает обратно внутрь уровня, и по конечной точке «перелетел стену
    // и был возвращён» неотличимо от «упёрся в стену и никуда не ходил».
    fix32 leftmost = st.hero.position.x;
    for (uint32_t t = 0; t < 120; ++t) {
        in.jump_held = (t % 40) < 20;
        platformer::step_stage(st, in);
        if (st.hero.position.x < leftmost) leftmost = st.hero.position.x;
    }
    std::printf("  edge:  hero=%.3f leftmost=%.3f left=%.3f ground=%d\n",
                st.hero.position.x.to_double(), leftmost.to_double(), left.to_double(),
                st.hero.on_ground ? 1 : 0);
    // Предпосылка: он реально ШЁЛ. Прогон, в котором ввод не доехал до контроллера, стоял бы на
    // точке появления и проходил бы утверждение о крае, ничего про край не сказав.
    check(st.hero.position.x < platformer::SPAWN_X, "precondition: the run really walks left");
    // Вторая предпосылка, и она про сам край: персонаж обязан оказаться ЗА стеной, то есть левее её
    // правой грани. Прогон, которого стена не пустила, зелен вакуумно — граница уровня в нём не
    // участвует вовсе.
    check(leftmost < left + st.grid->tile_size(),
          "precondition: the jump really carried the hero past the edge wall");
    check(!(leftmost < left), "walking into the left edge leaves the hero on the level");
    check(st.hero.on_ground, "and on its floor: past the edge there is nothing to stand on");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string path = DEFAULT_BUNDLE;
    bool trace = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--trace") == 0) trace = true;
        else path = argv[i];
    }

    std::printf("platformer sample: scripted run over the shipped level\n");

    platformer::Stage stage;
    if (!platformer::load_stage(path, stage)) {
        std::printf("  FAIL: level not readable from %s (sections 'tilemap' and 'movement')\n",
                    path.c_str());
        std::printf("game-platformer-sim: FAIL\n");
        return 1;
    }

    const platformer::RunResult run = platformer::run_script(stage, trace);
    std::printf("  ticks: %u, final: %.3f %.3f\n", run.ticks, run.last_position.x.to_double(),
                run.last_position.y.to_double());
    // Значение — отдельной строкой в домашнем формате, а не внутри сводки: его грепают Release-шаг
    // CI и Debug-этап, и расследование расхождения обязано начинаться с чтения лога, а не с
    // пересборки. Тем же способом стоят голдены физики, траектории персонажа и таблиц.
    std::printf("  platformer trajectory hash = 0x%016llx\n",
                static_cast<unsigned long long>(run.hash));

    // Маршрут ЗАДЕЛ каждый приём вертикали 3. Утверждения именно здесь, а не в комментарии к
    // скрипту: правка карты или числа тиков, уронившая персонажа мимо площадки, обязана валить
    // гейт, а не тихо перепечатывать хеш.
    check(run.seen.walked_slope, "the route walked a slope tile");
    check(run.seen.rose_through, "the route rose through a one-way platform from below");
    check(run.seen.stood_oneway, "the route stood on a one-way platform");
    check(run.seen.dropped_through, "down+jump dropped the hero through a one-way platform");
    check(run.seen.rode_lift, "the moving platform became the hero's support");
    check(run.seen.carried, "the moving platform carried the hero with no input of his own");
    check(run.seen.hit_ceiling, "the route hit a ceiling");
    check(run.seen.hit_wall, "the route hit a wall");
    check(!run.seen.crushed, "the route never crushes the hero");

    // Тот же уровень, прочитанный заново, даёт тот же прогон. Это не проверка ОС — это проверка
    // того, что в прогоне не осталось состояния, пережившего `Stage`: глобала, статика или
    // неинициализированного поля, из-за которого второй прогон в том же процессе уже другой.
    platformer::Stage again;
    check(platformer::load_stage(path, again), "the level loads a second time");
    const platformer::RunResult repeat = platformer::run_script(again, /*trace=*/false);
    check(repeat.hash == run.hash, "a second run of the same script hashes the same");

    if (run.hash != GOLDEN_HASH) {
        std::printf("  FAIL: golden 0x%016llx, got 0x%016llx\n",
                    static_cast<unsigned long long>(GOLDEN_HASH),
                    static_cast<unsigned long long>(run.hash));
        ++fails;
    }

    test_the_level_has_edges(path);

    std::printf("game-platformer-sim: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
