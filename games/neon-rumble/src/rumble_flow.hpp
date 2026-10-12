#pragma once
#include <cstdint>
#include <memory>

#include "bundle_view.hpp"
#include "rumble_credits.hpp"
#include "rumble_menu.hpp"
#include "rumble_play.hpp"
#include "scene_stack.hpp"

namespace rumble {

// Стек сцен игры: меню вне сессии отката и уровень, в который из него вошли. Переходы подаёт код
// игры после `run` — Enter из меню, Esc из уровня, — применяются они в начале следующего кадра.
// Уровень живёт, пока его сцена в стеке или в очереди; при следующем входе грузится заново.
class Flow {
public:
    Flow(const asset::BundleView& bundle, const Credits& credits, Menu& menu)
        : bundle_(bundle), credits_(credits), menu_(menu) {}

    bool open();
    // true — сменилась сцена на вершине: окну перезалить текстуры.
    bool apply();
    void run(uint32_t tick);
    // Вход в уровень пункта меню; отказ — строка `menu: <ошибка>` и текст над пунктами.
    void enter();
    void leave();

    // Уровень на вершине стека; nullptr, пока на вершине меню.
    Play* play() { return bound_ == &menu_ ? nullptr : play_.get(); }
    const Play* play() const { return bound_ == &menu_ ? nullptr : play_.get(); }
    Menu& menu() { return menu_; }

private:
    void refuse(const std::string& error);

    const asset::BundleView& bundle_;
    const Credits& credits_;
    Menu& menu_;
    framework::scene::SceneStack stack_;
    std::unique_ptr<Play> play_;
    const void* bound_ = nullptr;
};

} // namespace rumble
