#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platformer_view.hpp"

// Гейт ЧИСТОЙ половины живой цели `game_platformer` (шаг C вертикали 3 спеки #16): камера и сборка
// квадов проверяются без окна, без GLFW и без адаптера — то есть на всех трёх ОС, а не на машине
// владельца.
//
// Гейт нужен ровно потому, что окно проверить нечем: «камера доехала до края» и «склон нарисован
// ступенькой» — утверждения о числах, а не о картинке, и утонули бы в «на глаз выглядит правильно».
// Живому прогону остаётся то, что числами и не сказать: отзывчивость и то, что окно вообще
// открылось (гейт 8, `docs/owner-verification.md`).
namespace {

namespace pv = platformer;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

bool close_to(float a, float b) { return a - b < 0.002f && b - a < 0.002f; }

// Камера при персонаже, поставленном в точку. Позиция задаётся прямо, а не прогоном скрипта: вопрос
// здесь про кламп на КРАЯХ, а маршрут до края ещё надо суметь довести — и он проверяется другим
// гейтом (`game_platformer_sim_test`).
pv::Camera camera_with_hero(pv::Stage& st, int x, int y) {
    st.hero.position = {fix32::from_int(x), fix32::from_int(y)};
    return pv::camera_at(st);
}

const pv::Quad* find_color(const std::vector<pv::Quad>& qs, pv::Rgba c) {
    for (const pv::Quad& q : qs)
        if (q.color == c) return &q;
    return nullptr;
}

// Первый тайл-склон без зеркал (`slope_br`): прямой угол внизу справа, гипотенуза из нижнего левого
// в верхний правый. Ищется В СЕТКЕ, а не выписан индексом: индекс продублировал бы карту, и правка
// `tilemap.txt` тихо увела бы гейт на соседний тайл.
bool find_slope_br(const pv::Stage& st, int32_t& tx, int32_t& ty) {
    for (uint32_t y = 0; y < st.grid->height(); ++y)
        for (uint32_t x = 0; x < st.grid->width(); ++x) {
            const auto fl = st.grid->at(static_cast<int32_t>(x), static_cast<int32_t>(y));
            if ((fl & pv::tm::TILE_SLOPE) == 0) continue;
            if ((fl & (pv::tm::TILE_SLOPE_FLIP_X |
                       pv::tm::TILE_SLOPE_FLIP_Y)) != 0) continue;
            tx = static_cast<int32_t>(x);
            ty = static_cast<int32_t>(y);
            return true;
        }
    return false;
}

void check_camera(pv::Stage& st) {
    const int wide = static_cast<int>(st.grid->width()) * st.grid->tile_size().to_int();
    const int tall = static_cast<int>(st.grid->height()) * st.grid->tile_size().to_int();

    // Оба КОНЦА, а не середина: кламп — это поведение на границах, и голден из середины диапазона
    // одинаков у клампа и у его отсутствия.
    const pv::Camera at_left = camera_with_hero(st, 40, 200);
    check(at_left.left.raw == 0, "camera clamps to the left edge of the level");
    const pv::Camera at_right = camera_with_hero(st, wide - 25, 200);
    check(at_right.left == fix32::from_int(wide - pv::VIEW_W),
          "camera clamps to the right edge of the level");
    const pv::Camera mid = camera_with_hero(st, 300, 200);
    check(mid.left == fix32::from_int(300 - pv::VIEW_W / 2),
          "camera follows the hero in the middle");

    // Уровень ровно в высоту вида: кламп обязан прижать камеру к нулю и на дне, и под потолком.
    // Утверждение про КАРТУ, а не про камеру, стоит рядом — иначе строка ниже молчит про то, что
    // проверяет вырожденный случай.
    check(tall == pv::VIEW_H, "the level is exactly one view tall");
    check(camera_with_hero(st, 300, 16).top.raw == 0, "camera clamps to the top of the level");
    check(camera_with_hero(st, 300, 224).top.raw == 0, "camera clamps to the bottom of the level");
}

void check_slope(pv::Stage& st, const pv::Camera& cam, const std::vector<pv::Quad>& qs) {
    int32_t tx = 0, ty = 0;
    if (!find_slope_br(st, tx, ty)) {
        check(false, "the shipped level has a slope_br tile to draw");
        return;
    }
    const pv::ph::Aabb box = st.grid->tile_bounds(tx, ty);
    const float left = static_cast<float>((box.min.x - cam.left).to_double());
    const float top = static_cast<float>((box.min.y - cam.top).to_double());
    const float size = static_cast<float>((box.max.x - box.min.x).to_double());
    const float step = size / pv::SLOPE_STEPS;

    // Подквады ЭТОГО тайла, слева направо. Отбираются по коробке тайла: цвет общий у всех склонов
    // карты, и по нему сюда попал бы соседний холм.
    std::vector<const pv::Quad*> sub;
    for (const pv::Quad& q : qs)
        if (q.x >= left - 0.002f && q.x + q.w <= left + size + 0.002f &&
            q.y >= top - 0.002f && q.y + q.h <= top + size + 0.002f)
            sub.push_back(&q);

    const size_t steps = static_cast<size_t>(pv::SLOPE_STEPS);
    check(sub.size() == steps, "a slope tile is drawn as a column of sub-quads");
    if (sub.size() != steps) return;
    bool rises = true, off_floor = false, inside = true;
    for (size_t i = 0; i < sub.size(); ++i) {
        const pv::Quad& q = *sub[i];
        if (q.color != pv::C_SLOPE) inside = false;
        if (!close_to(q.w, step)) inside = false;
        if (!close_to(q.x, left + step * static_cast<float>(i))) inside = false;
        if (!close_to(q.y + q.h, top + size)) off_floor = true;   // низ подступеньки — дно тайла
        if (i && sub[i - 1]->h > q.h + 0.002f) rises = false;
    }
    check(inside, "every sub-quad stands in its own column inside the tile");
    check(!off_floor, "every sub-quad reaches the bottom of the tile");
    check(rises, "the sub-quads grow left to right - the hypotenuse of slope_br rises that way");
    check(close_to(sub.front()->h, step), "the low end of the slope is one step tall");
    check(close_to(sub.back()->h, size), "the high end of the slope fills the tile");
}

void check_frame(pv::Stage& st) {
    // Камера у левого края: в кадре пол, обе односторонние площадки и стартовая стена, но НЕ правый
    // конец карты — на нём и проверяется отбрасывание.
    const pv::Camera cam = camera_with_hero(st, 40, 200);
    std::vector<pv::Quad> qs;
    pv::build_quads(st, cam, qs);
    check(!qs.empty(), "the frame is not empty");

    bool outside = false;
    for (const pv::Quad& q : qs)
        if (q.x + q.w <= 0 || q.y + q.h <= 0 || q.x >= pv::VIEW_W || q.y >= pv::VIEW_H)
            outside = true;
    check(!outside, "nothing outside the view takes a slot in the batch");

    const pv::Quad* hero = find_color(qs, pv::C_HERO);
    check(hero != nullptr, "the hero is drawn");
    if (hero)
        check(close_to(hero->w, 16) && close_to(hero->h, 32) &&
                  close_to(hero->x, 32) && close_to(hero->y, 184),
              "the hero quad is his hull, placed against the camera");

    check(find_color(qs, pv::C_ONEWAY) != nullptr, "a one-way platform is drawn in its own colour");
    check(find_color(qs, pv::C_SOLID) != nullptr, "a solid tile is drawn in its own colour");

    // Платформа стоит за правым краем этого вида — её отсутствие здесь и есть отбрасывание. Второй
    // кадр, снятый оттуда, обязан её показать: без него «нет платформы» доказывало бы, что её не
    // рисуют вовсе.
    check(find_color(qs, pv::C_LIFT) == nullptr, "the far-off lift takes no slot near the spawn");
    const pv::Camera far = camera_with_hero(st, 540, 176);
    std::vector<pv::Quad> there;
    pv::build_quads(st, far, there);
    const pv::Quad* lift = find_color(there, pv::C_LIFT);
    check(lift != nullptr, "the lift is drawn once the camera reaches it");
    if (lift)
        check(close_to(lift->w, 48) && close_to(lift->h, 16), "the lift quad is the plate itself");

    check_slope(st, cam, qs);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string path = DEFAULT_BUNDLE;
    for (int i = 1; i < argc; ++i) path = argv[i];

    std::printf("platformer view: camera and quads over the shipped level\n");

    pv::Stage stage;
    if (!pv::load_stage(path, stage)) {
        std::printf("  FAIL: level not readable from %s\n", path.c_str());
        std::printf("game-platformer-view: FAIL\n");
        return 1;
    }
    check(stage.grid.has_value(), "the level came with a grid");
    if (stage.grid) {
        check_camera(stage);
        check_frame(stage);
    }

    std::printf("game-platformer-view: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
