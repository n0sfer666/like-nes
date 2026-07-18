#include "scene.hpp"

#include <cmath>

void Scene::advance() {
    // Симуляционный тик — только fix32, детерминированно, без float.
    phase = phase + fix32::from_float(0.016);
}

SceneSnapshot Scene::snapshot(float aspect) const {
    // Граница рендера: единственная fix32→float конверсия.
    const float t = static_cast<float>(phase.to_double());

    SceneSnapshot s = {};
    s.aspect = aspect;
    s.opaque = SpriteXform{0.0f, 0.0f, 0.62f, 0.0f};
    s.glow = SpriteXform{0.34f * std::sin(t * 0.9f), 0.22f, 0.40f, t * 0.6f};
    s.light_count = 3;

    const float col[3][3] = {
        {1.00f, 0.42f, 0.28f},
        {0.30f, 0.55f, 1.00f},
        {0.45f, 1.00f, 0.55f},
    };
    for (int i = 0; i < 3; ++i) {
        const float a = t * (0.7f + 0.18f * i) + i * 2.094f;
        const float rad = 0.60f + 0.12f * std::sin(t * 0.5f + i);
        s.lights[i] = LightState{
            rad * std::cos(a), rad * std::sin(a), 0.35f,
            col[i][0], col[i][1], col[i][2],
            1.35f, 1.1f,
        };
    }
    return s;
}
