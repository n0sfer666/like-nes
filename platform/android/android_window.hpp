#pragma once
#include <android/native_activity.h>

#include <string>

// Снимок UI-потока: пиксели окна, до перевода в точки с плотностью AConfiguration.
struct AndroidWindowSnapshot {
    int left = 0, top = 0, right = 0, bottom = 0;
    int rotation = -1;
    bool insets = false;
    std::string probe;
    bool operator==(const AndroidWindowSnapshot&) const = default;
};

// JNI и View живут на UI-потоке. Запрос через его looper не задерживает игровой поток;
// следующий read забирает готовый снимок. Оконные события также обновляют его до native_app_glue.
AndroidWindowSnapshot android_window_read(ANativeActivity* activity);
void android_window_request(ANativeActivity* activity);
