#include <cstdio>
#include <string>
#include <vector>

#include "bundle_asset.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "touch_layout.hpp"

// Второе утверждение о секции `touch` бандла, первым не заменяемое: `--verify-game` сверяет БАЙТЫ с
// перепечённым `touch.txt` и читателя не запускает, а мобильный шутер без читаемой раскладки не
// стартует вовсе (`MobileGame::init`). Здесь секция идёт ТЕМ ЖЕ путём, что у игры —
// `read_bundle_asset` + `read_layout`, — и сверяется с разбором исходника, независимым от байт.
namespace {

using framework::touch::ButtonSpec;
using framework::touch::Layout;

int fails = 0;
int cases = 0;

void check(bool ok, const char* what) {
    ++cases;
    if (!ok) { std::printf("  FAIL: %s\n", what); ++fails; }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

bool same(const Layout& a, const Layout& b) {
    if (!(a.stick_radius == b.stick_radius) || a.button_count != b.button_count) return false;
    for (uint32_t i = 0; i < a.button_count; ++i) {
        const ButtonSpec& x = a.buttons[i];
        const ButtonSpec& y = b.buttons[i];
        if (x.button != y.button || x.anchor != y.anchor || !(x.dx == y.dx) || !(x.dy == y.dy) ||
            !(x.radius == y.radius))
            return false;
    }
    return true;
}

std::string dir_of(const std::string& path) {
    const std::string::size_type slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("touch section of game.bundle against touch.txt\n");
    const std::string bundle = argc > 1 ? argv[1] : DEFAULT_BUNDLE;

    std::vector<uint8_t> bytes;
    Layout baked;
    const bool read = game::read_bundle_asset(bundle, "touch", bytes);
    check(read, "the bundle holds a 'touch' section (rebake it with ugly_assetc --game)");
    check(read && framework::touch::read_layout(bytes.data(), bytes.size(), baked),
          "the section reads as a touch layout");

    const std::string src = dir_of(bundle) + "/touch.txt";
    std::string text, error;
    Layout parsed;
    check(platform::read_text(src, text) && framework::touch::parse_layout(text, src, parsed, error),
          "touch.txt next to the bundle parses");
    if (!error.empty()) std::printf("  %s\n", error.c_str());

    check(same(baked, parsed), "the baked layout equals touch.txt");
    // Шутер рисует и ловит кнопку 0 и стик: раскладка без них проходит сверку выше, а игра без них
    // не стреляет и не ходит пальцем.
    check(baked.stick_radius.raw > 0, "the layout has a stick");
    check(baked.button_count > 0 && baked.buttons[0].button == 0, "the layout has button a");

    // Позитивный контроль сверки: сдвиг кнопки на одну точку обязан быть отбит тем же сравнением,
    // иначе «равны» неотличимо от сравнения, которое не сравнивает.
    Layout moved = parsed;
    moved.buttons[0].dy = moved.buttons[0].dy + fix32::from_int(1);
    check(parsed.button_count > 0 && !same(moved, parsed), "the comparison tells a one-point shift apart");

    std::printf("game-touch-bundle: %s - %d cases\n", fails == 0 ? "PASS" : "FAIL", cases);
    return fails == 0 ? 0 : 1;
}
