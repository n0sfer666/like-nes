#pragma once
#include <cstddef>
#include <cstdint>

#include "param.hpp"

namespace mat {

// Zero-parse таблица материалов: рантайм читает её прямо из mmap-региона бандла, как достижения
// (#10) и пресеты ввода (#14). Раскладка запинена static_assert'ами — это ABI между пекарем и
// читателем, и разъехаться молча ей нечем.
constexpr uint8_t TABLE_MAGIC[4] = {'L', 'N', 'M', 'T'};
constexpr uint32_t TABLE_VERSION = 1;

constexpr uint16_t NO_BASE = 0xffffu;

// Предел глубины наследования. Он ОДИН на читателя и на проверку: `load` отбивает цепь длиннее
// этого числа кодом `BadBase`, а `resolve`/`slot_of` обходят ровно столько же. Пока предел жил
// литералом `64` в двух обходах, а `load` мерил цепь числом материалов, таблица с 65-уровневым
// наследованием считалась валидной и МОЛЧА теряла параметры корней — отказа нет, есть искажение.
constexpr uint32_t MAX_BASE_DEPTH = 64;

enum class Blend : uint8_t { Opaque = 0, Alpha = 1, Additive = 2 };

struct TableHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t material_count;
    uint32_t param_count;
    uint32_t texture_count;
    uint32_t materials_offset;
    uint32_t params_offset;
    uint32_t textures_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(TableHeader) == 40, "TableHeader layout pinned (zero-parse ABI)");

struct MaterialRow {
    uint64_t shader_guid;
    uint32_t name_off;
    uint32_t param_first;
    uint32_t texture_first;
    uint16_t param_count;
    uint16_t texture_count;
    uint16_t base;
    uint8_t blend;
    uint8_t reserved0;
    // Имя шейдера, а не только его guid: guid адресует ассет, а точку входа в модуле нечем назвать,
    // кроме строки. Отдельного соглашения «guid X значит вход Y» здесь нет намеренно — соглашение,
    // которое ничто не проверяет, разъезжается молча (спека #18, шов «материал → пайплайн»).
    uint32_t shader_off;
};
static_assert(sizeof(MaterialRow) == 32, "MaterialRow layout pinned (zero-parse ABI)");

struct ParamRow {
    uint32_t name_off;
    float value[4];
    uint8_t type;
    uint8_t unit;
    uint8_t slot;
    uint8_t reserved;
};
static_assert(sizeof(ParamRow) == 24, "ParamRow layout pinned (zero-parse ABI)");

struct TextureRow {
    uint64_t guid;
    uint32_t name_off;
    uint8_t binding;
    uint8_t reserved[3];
};
static_assert(sizeof(TextureRow) == 16, "TextureRow layout pinned (zero-parse ABI)");

enum class LoadResult : uint32_t {
    Ok = 0,
    TooShort = 1,
    BadMagic = 2,
    BadVersion = 3,
    BadLayout = 4,
    BadString = 5,
    BadRange = 6,
    BadSlot = 7,
    BadBase = 8,
    BadEnum = 9,
};

const char* load_reason(LoadResult r);

// Вид на таблицу. Владения байтами нет: они живут в mmap-регионе бандла и переживают вид.
class Table {
public:
    LoadResult load(const void* base, std::size_t size);

    uint32_t count() const { return header_ ? header_->material_count : 0; }
    const MaterialRow& row(uint32_t i) const { return materials_[i]; }
    const ParamRow& param(uint32_t i) const { return params_[i]; }
    const TextureRow& texture(uint32_t i) const { return textures_[i]; }
    const char* name(uint32_t off) const { return strings_ + off; }

    // Имя шейдера материала = имя точки входа в модуле библиотеки.
    const char* shader(uint32_t i) const { return strings_ + materials_[i].shader_off; }

    // Индекс материала по имени — то самое число, которое ложится в `Sprite::material`.
    // Ненайденное имя отдаёт `count()`, а не 0: нулевой индекс — законный материал.
    uint32_t find(const char* material_name) const;

    // Значения параметров материала с учётом базы: инстанс наследует блок базового материала и
    // переопределяет свои слоты. Развёртка делается ЗДЕСЬ, а не у потребителя, — иначе каждый
    // потребитель наследовал бы по-своему (решение 2 спеки #18).
    void resolve(uint32_t i, float out[PARAM_BLOCK_FLOATS]) const;

    // Смещение параметра по ИМЕНИ, с той же развёрткой базы. Нет параметра — `-1`.
    // Выставлено наружу затем, чтобы игра, меняющая силу вспышки в кадре, не писала смещение
    // числом: перестановка строк в `library.mat` тогда молча красила бы не тот слот, и увидеть
    // это можно было бы только глазами на кадре (тот же класс, что ловит material_library_test
    // для WGSL библиотеки).
    int32_t slot_of(uint32_t i, const char* param_name) const;

private:
    const TableHeader* header_ = nullptr;
    const MaterialRow* materials_ = nullptr;
    const ParamRow* params_ = nullptr;
    const TextureRow* textures_ = nullptr;
    const char* strings_ = nullptr;
};

} // namespace mat
