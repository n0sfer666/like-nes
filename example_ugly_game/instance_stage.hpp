#pragma once
#include <cstdint>

#include "instance.hpp"

namespace game {

// Накопитель кадра ПЕРЕД загрузкой в вершинный буфер (спека #17, вертикаль 3, шаг B3). Отдельным
// типом от `SpriteBatch`, а не полем внутри него, ровно затем, чтобы утверждение «установившийся
// кадр не ходит в кучу» можно было проверить БЕЗ WebGPU: `SpriteBatch` открывает устройство,
// текстуру и конвейер, и гейт на нём был бы гейтом на видеокарте раннера, которой там нет.
//
// Память ВЛАДЕЕТСЯ ВЫЗЫВАЮЩИМ — та же договорённость, что у `ParticleStore` и `SpriteList` во
// фреймворке, и по той же причине: у накопителя нет ни одного основания знать, откуда взялся
// буфер, а у потребителя есть — размер кадра он выбирает сам.
//
// Переполнение ОТКАЗЫВАЕТ и считается, а не растит буфер. Рост в кадре — это и есть та аллокация,
// которую запрещает гейт 8, и «тихо выросли, зато нарисовали всё» здесь хуже потерянного спрайта:
// первое узнаётся профилировщиком через полгода, второе видно в числе на том же кадре.
class InstanceStage {
public:
    InstanceStage(Instance* buf, uint32_t capacity) : buf_(buf), capacity_(capacity) {}

    void begin() { count_ = 0; dropped_ = 0; }
    void push(const Instance& inst);

    const Instance* data() const { return buf_; }
    uint32_t count() const { return count_; }
    uint32_t dropped() const { return dropped_; }
    uint32_t capacity() const { return capacity_; }
    uint32_t bytes() const { return count_ * static_cast<uint32_t>(sizeof(Instance)); }

private:
    Instance* buf_ = nullptr;
    uint32_t capacity_ = 0;
    uint32_t count_ = 0;
    uint32_t dropped_ = 0;
};

} // namespace game
