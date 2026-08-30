#include "debug_draw.hpp"

namespace framework::graphics {
namespace {

const fix32 HALF = fix32::from_raw(fix32::ONE / 2);

// Пропорции глифа взяты у игры-образца (`draw.cpp`: dw = cell * 0.72, dh = cell * 1.05) — шаг E
// ИЗВЛЕКАЕТ найденное живым прогоном, а не изобретает заново (решение 1 спеки). Записаны они
// рациональными дробями, а не десятичной записью: 9/25 и 21/40 точны в любой системе счисления,
// и вопрос «а как это округлилось у них» не возникает вовсе.
fix32 glyph_half_w(fix32 cell) { return cell * fix32::from_int(9) / fix32::from_int(25); }
fix32 glyph_half_h(fix32 cell) { return cell * fix32::from_int(21) / fix32::from_int(40); }

bool glyph_of(const DebugGlyphs& g, char c, RegionId& out) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') {
        out = g.letter[c - 'A'];
        return true;
    }
    if (c >= '0' && c <= '9') {
        out = g.digit[c - '0'];
        return true;
    }
    return false;
}

} // namespace

// Имена глифов те же, что в `example_ugly_game/assets/atlas.txt`, и это соглашение, а не совпадение:
// шрифт оверлея — часть атласа игры, и держать для него второй словарь имён значило бы завести
// второй источник правды о той же картинке.
bool resolve_debug_glyphs(const AtlasTable& table, DebugGlyphs& out) {
    out = DebugGlyphs{};
    if (!table.valid()) return false;
    if (const std::optional<RegionId> id = table.find("solid")) {
        out.solid = *id;
        out.has_solid = true;
    }
    char name[10] = "digit_0";
    bool text = true;
    for (int d = 0; d < 10 && text; ++d) {
        name[6] = static_cast<char>('0' + d);
        const std::optional<RegionId> id = table.find(name);
        text = id.has_value();
        if (text) out.digit[d] = *id;
    }
    char letter[11] = "letter_a";
    for (int l = 0; l < 26 && text; ++l) {
        letter[7] = static_cast<char>('a' + l);
        const std::optional<RegionId> id = table.find(letter);
        text = id.has_value();
        if (text) out.letter[l] = *id;
    }
    out.has_text = text;
    return out.has_solid;
}

DebugDraw::DebugDraw(DebugQuad* storage, uint32_t capacity, const DebugGlyphs& glyphs)
    : storage_(storage), capacity_(storage != nullptr ? capacity : 0), glyphs_(glyphs) {}

void DebugDraw::clear() {
    count_ = 0;
    dropped_ = 0;
}

void DebugDraw::push(Vec2 center, Vec2 half, Vec2 dir, uint32_t rgba, RegionId region) {
    if (count_ >= capacity_) {
        ++dropped_;
        return;
    }
    DebugQuad& q = storage_[count_++];
    q.center = center;
    q.half = half;
    q.dir = dir;
    q.rgba = rgba;
    q.region = region;
}

void DebugDraw::line(Vec2 a, Vec2 b, fix32 thickness, uint32_t rgba) {
    if (!glyphs_.has_solid || thickness.raw <= 0) {
        ++dropped_;
        return;
    }
    Vec2 dir{};
    const fix32 len = normalize(b - a, dir);
    // Отрезок нулевой длины отбивается, а не рисуется точкой: направление у него не определено, и
    // единичный вектор пришлось бы выдумать — потребитель развернул бы квад в произвольную сторону.
    if (len.raw <= 0) {
        ++dropped_;
        return;
    }
    push((a + b) * HALF, {len * HALF, thickness * HALF}, dir, rgba, glyphs_.solid);
}

void DebugDraw::fill(Vec2 center, Vec2 half, uint32_t rgba) {
    if (!glyphs_.has_solid) {
        ++dropped_;
        return;
    }
    push(center, half, {fix32::from_int(1), fix32{}}, rgba, glyphs_.solid);
}

void DebugDraw::frame(Vec2 center, Vec2 half, fix32 thickness, uint32_t rgba) {
    if (!glyphs_.has_solid || thickness.raw <= 0) {
        dropped_ += 4;
        return;
    }
    const fix32 t = thickness * HALF;
    fill({center.x, center.y - half.y + t}, {half.x, t}, rgba);
    fill({center.x, center.y + half.y - t}, {half.x, t}, rgba);
    // Перекладины короче на толщину с каждого конца: угол уже покрыт горизонталями. Выродиться они
    // могут законно — у рамки толщиной в свою высоту вертикалей нет вовсе, и это не потеря, а
    // отсутствие незакрытого места. Поэтому вырождение НЕ считается невыданным квадом.
    const fix32 inner = half.y - thickness;
    if (inner.raw <= 0) return;
    fill({center.x - half.x + t, center.y}, {t, inner}, rgba);
    fill({center.x + half.x - t, center.y}, {t, inner}, rgba);
}

void DebugDraw::text(const char* s, Vec2 at, fix32 cell, uint32_t rgba) {
    if (s == nullptr) return;
    const Vec2 dir{fix32::from_int(1), fix32{}};
    const Vec2 half{glyph_half_w(cell), glyph_half_h(cell)};
    fix32 x = at.x;
    for (int i = 0; s[i] != '\0'; ++i, x = x + cell) {
        RegionId region = 0;
        if (!glyph_of(glyphs_, s[i], region)) continue;   // пробел и знаки двигают курсор молча
        if (!glyphs_.has_text) {
            ++dropped_;
            continue;
        }
        push({x, at.y}, half, dir, rgba, region);
    }
}

} // namespace framework::graphics
