#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>

// Карты нормалей гейта 7 считаются в коде по тем же причинам, что и картинки сцены материалов:
// движковому гейту нужен кадр, воспроизводимый на любой машине, а файл привязал бы его к бандлу
// игры-образца. Настоящий загрузчик текстур — предмет ассетной вертикали, а не этой.
//
// КОНТРАКТ ЗНАКА, записанный здесь один раз: карта хранит нормаль в базисе ПРОХОДА ОСВЕЩЕНИЯ —
// +X вправо, +Y ВВЕРХ, +Z на зрителя, — закодированную как `n * 0.5 + 0.5`. Текстурная координата
// растёт вниз, поэтому переворот знака Y делается ровно в одном месте, в генераторе. Не записанное
// соглашение о знаке каждый потребитель пишет по-своему, и расходятся они молча — светом с изнанки.
namespace normgold {

// Банк карт по GUID АССЕТА: таблица хранит текстурный слот именно хешем имени, строку знает только
// `library.mat`. Неизвестный guid отдаёт nullptr, а не плоскую карту: «ассета нет» и «материал не
// просил нормаль» обязаны различаться, иначе опечатка в `library.mat` читается как замысел.
class Bank {
public:
    bool init(WGPUDevice device, WGPUQueue queue);
    void shutdown();

    WGPUTextureView view(uint64_t guid) const;
    const char* name(uint64_t guid) const;
    WGPUTextureView flat() const { return flat_view_; }

private:
    WGPUTexture flat_ = nullptr;
    WGPUTexture dome_ = nullptr;
    WGPUTexture ridge_ = nullptr;
    WGPUTextureView flat_view_ = nullptr;
    WGPUTextureView dome_view_ = nullptr;
    WGPUTextureView ridge_view_ = nullptr;
};

} // namespace normgold
