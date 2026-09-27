#pragma once

#include <cstdint>
#include <string>

#include "hot_reload.hpp"
#include "material_fx.hpp"

namespace game {

// Дев-режим гейта 3 спеки #18. Путь к ИСХОДНИКУ модуля приходит снаружи и в бандле его нет:
// бандл — байты, править в нём нечего, а игра-образец обязана уметь то же, что редактор.
// Наблюдение не включено по умолчанию: игрок правок шейдера не делает, а вотч на каталог —
// дескриптор и опрос каждый кадр.
class ShaderWatch {
public:
    bool start(const MaterialFx& materials, const std::string& wgsl_path);
    // Зовётся из кадра БЕЗ ожидания: кадр здесь стоит 16 мс, и любое окно ожидания вычитается
    // прямо из него. Отказ битой правки не гасит сцену — рисует прежний вариант.
    void poll(MaterialFx& materials);

    bool watching() const { return hot_.watching(); }
    const char* backend() const {
        return hot_.backend() == platform::WatchBackend::Native ? "native" : "poll";
    }
    const char* error() const { return hot_.error().c_str(); }
    uint32_t rejects() const { return hot_.rejects(); }

private:
    mat::HotReload hot_;
};

} // namespace game
