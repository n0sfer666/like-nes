#pragma once
#include "../core/fixed.hpp"

// Спрайт-трансформ в render-пространстве (float — только на границе рендера).
struct SpriteXform { float x, y, scale, rot; };

// Динамический источник света: позиция (z = высота над плоскостью спрайта), цвет, сила, радиус.
struct LightState { float x, y, z; float r, g, b; float intensity; float radius; };

// Снапшот сцены на границе рендера — единственное место float-конверсии.
struct SceneSnapshot {
    SpriteXform opaque;   // непрозрачный нормал-мапленный спрайт (deferred)
    SpriteXform glow;     // полупрозрачный спрайт (forward, шаг 3)
    LightState lights[3];
    int light_count;
    float aspect;
};

// Sim-домен: детерминированный счётчик в fix32. advance() — только fix32, без float,
// без GPU→sim readback (инвариант «GPU не кормит сим»). snapshot() конвертирует в float
// ТОЛЬКО на границе рендера (орбиты светов — визуальные, недетерминированность допустима).
struct Scene {
    fix32 phase;
    void advance();
    SceneSnapshot snapshot(float aspect) const;
};
