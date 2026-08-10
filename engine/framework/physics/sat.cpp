#include "sat.hpp"

#include <cstdint>

#include "gap.hpp"

namespace framework::physics {
namespace {

// Смещение в пользу опорной грани первого тела. Без него пара, у которой две глубины совпадают до
// младшего разряда, выбирает опорную гранью то одну форму, то другую от кадра к кадру: нормаль
// прыгает, идентификаторы точек меняются, тёплый старт каждый раз начинает с нуля — и ящик на полу
// мелко дрожит. Порог не «эпсилон для равенства», а гистерезис: он обязан быть заметно больше
// шага fix32, иначе не удерживает выбор.
constexpr fix32 FACE_BIAS = fix32::from_float(0.03125);

struct FaceQuery {
    fix32 separation;
    uint8_t index = 0;
};

// Самая «мелкая» грань `a` относительно `b`: максимум по граням от минимума по вершинам. Именно
// максимум — теорема о разделяющей оси утверждает, что формы разъединены, если НАЙДЕТСЯ ось с
// зазором, поэтому искать надо лучшую для разъединения, а не худшую.
FaceQuery deepest_face(const WorldShape& a, const WorldShape& b) {
    FaceQuery best{fix32::from_raw(INT32_MIN), 0};
    for (uint8_t i = 0; i < a.count; ++i) {
        const Vec2 n = a.normals[i];
        fix32 nearest = fix32::from_raw(INT32_MAX);
        for (uint8_t j = 0; j < b.count; ++j) {
            nearest = min_fix(nearest, dot(n, b.points[j] - a.points[i]));
        }
        // Радиусы скругления ОБЕИХ форм уменьшают зазор: ядро отстоит от поверхности на радиус, и
        // проекция считалась по ядрам. Отсюда же берётся капсула — её грань это отрезок ядра.
        const fix32 separation = nearest - a.radius - b.radius;
        if (best.separation < separation) best = {separation, i};
    }
    return best;
}

// Падающая грань — самая «встречная» опорной: минимум скалярного произведения нормалей. Не
// ближайшая по расстоянию: расстояние выбирает грань, которая в пересечении может вообще не
// участвовать, и отсечение тогда даёт точки на другой стороне формы.
uint8_t incident_face(const WorldShape& s, Vec2 ref_normal) {
    uint8_t best = 0;
    fix32 lowest = fix32::from_raw(INT32_MAX);
    for (uint8_t i = 0; i < s.count; ++i) {
        const fix32 facing = dot(s.normals[i], ref_normal);
        if (facing < lowest) {
            lowest = facing;
            best = i;
        }
    }
    return best;
}

// Отсечение отрезка полуплоскостью dot(n, p - origin) <= 0. Порядок точек на выходе сохраняет
// порядок на входе: идентификатор точки контакта берёт её место в списке, и перестановка местами
// означала бы переезд накопленного импульса с одного угла на другой.
uint8_t clip(Vec2 p0, Vec2 p1, Vec2 n, Vec2 origin, Vec2* out) {
    const fix32 d0 = dot(n, p0 - origin);
    const fix32 d1 = dot(n, p1 - origin);
    uint8_t count = 0;
    if (d0.raw <= 0) out[count++] = p0;
    // Пересечение считается только при СТРОГО разных знаках. Ноль на границе означает, что точка
    // уже лежит на плоскости и уже сохранена выше; добавить её же ещё раз как «пересечение» значило
    // бы получить манифольд из двух совпавших точек, то есть двойной импульс в одном месте.
    if ((d0.raw < 0 && d1.raw > 0) || (d0.raw > 0 && d1.raw < 0)) {
        out[count++] = p0 + (p1 - p0) * (d0 / (d0 - d1));
    }
    if (d1.raw <= 0) out[count++] = p1;
    return count;
}

// Причина точки контакта: чья грань опорная, какая об какую и место в отсечённом отрезке. Кодируется
// в одно число, потому что сравнивается оно только на равенство — при поиске той же точки в
// манифольде прошлого кадра.
constexpr uint32_t feature_id(bool flip, uint8_t ref, uint8_t inc, uint8_t slot) {
    return (static_cast<uint32_t>(flip) << 24) | (static_cast<uint32_t>(ref) << 16) |
           (static_cast<uint32_t>(inc) << 8) | slot;
}

// Отсечение падающей грани об опорную. Возвращает НЕ «есть ли контакт», а «выразило ли его
// отсечение»: пустой выход означает, что отвечать должен другой путь, и различать эти два ответа
// обязан вызывающий.
bool clip_faces(const WorldShape& ref, uint8_t ref_index, const WorldShape& inc, bool flip,
                Vec2 center_a, Vec2 center_b, fix32 margin, Manifold& out) {
    const Vec2 ref_normal = ref.normals[ref_index];
    const Vec2 ref_start = ref.points[ref_index];
    const Vec2 ref_end = ref.points[(ref_index + 1) % ref.count];
    Vec2 tangent;
    // Вырожденное ребро опорной формы — не аварийная ситуация, а форма, схлопнувшаяся клампами.
    // Отсекать по нему нечем.
    if (normalize(ref_end - ref_start, tangent).raw == 0) return false;

    const uint8_t inc_index = incident_face(inc, ref_normal);
    const Vec2 inc_start = inc.points[inc_index];
    const Vec2 inc_end = inc.points[(inc_index + 1) % inc.count];

    // Отсечение боковыми плоскостями опорной грани: всё, что вылезло за её концы, к этой грани
    // отношения не имеет. Две полуплоскости, а не одна: отрезок может торчать с обеих сторон.
    Vec2 stage[MAX_MANIFOLD_POINTS];
    const uint8_t staged = clip(inc_start, inc_end, -tangent, ref_start, stage);
    if (staged == 0) return false;
    Vec2 kept[MAX_MANIFOLD_POINTS];
    uint8_t count = 0;
    if (staged == 1) {
        // Одна точка на выходе первой плоскости — это касание её ровно по краю; отрезка для второго
        // отсечения уже нет, остаётся проверить единственную точку.
        if (dot(tangent, stage[0] - ref_end).raw <= 0) kept[count++] = stage[0];
    } else {
        count = clip(stage[0], stage[1], tangent, ref_end, kept);
    }

    out.normal = flip ? -ref_normal : ref_normal;
    out.count = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const fix32 separation = dot(ref_normal, kept[i] - ref_start) - ref.radius - inc.radius;
        // Точка, попавшая в боковые плоскости, но ушедшая от грани дальше спекулятивного поля, — не
        // контакт: грань конечна не только вдоль, но и поперёк. Отбор ПОТОЧЕЧНЫЙ, и это важнее, чем
        // кажется: у наклонённого ящика ближний угол уже в поле, а дальний ещё в нескольких юнитах,
        // и общий отбор по манифольду выдал бы вторую точку с зазором, который решатель принял бы за
        // разрешённое сближение на этом углу.
        if (margin < separation) continue;

        // Точка контакта — СЕРЕДИНА между поверхностями, а не точка на одной из них. Ядро отстоит
        // от поверхности на радиус, и взять поверхность падающей формы значило бы сместить плечо на
        // половину проникновения в её сторону: у пары «капсула об стену» это заметный перекос
        // момента, и вращение начинало бы зависеть от того, какая форма оказалась опорной.
        const Vec2 point =
            kept[i] - ref_normal * (inc.radius + fix32::from_raw(separation.raw / 2));

        ManifoldPoint& p = out.points[out.count];
        p = ManifoldPoint{};
        p.anchor_a = point - center_a;
        p.anchor_b = point - center_b;
        p.penetration = -separation;
        p.id = feature_id(flip, ref_index, inc_index, i);
        ++out.count;
    }
    return out.count > 0;
}

// Одна точка на ОПОРНОЙ ОСИ — ответ там, где отсечение не выразило ничего, а пересечение ядер уже
// доказано перебором граней. Путь зазора здесь не годится в принципе: он меряет расстояние перебором
// вершин, а у ПЕРЕСЕКАЮЩИХСЯ множеств ближайшая пара сидит на скрещении рёбер, и ни одна вершина о
// ней не знает — крест из двух брусков даёт там восемнадцать юнитов на очевидном перекрытии, то есть
// нормаль в дальний угол и нулевую глубину. Поэтому обе величины берутся оттуда, где они ДОКАЗАНЫ:
// глубину и направление даёт зазор опорной грани, а точку — глубочайшая вершина падающей (у
// выпуклого многоугольника опорная вершина в направлении -n лежит на самой встречной грани, и
// `incident_face` выбирает именно её).
bool axis_contact(const WorldShape& ref, uint8_t ref_index, const WorldShape& inc, fix32 separation,
                  bool flip, Vec2 center_a, Vec2 center_b, Manifold& out) {
    const Vec2 ref_normal = ref.normals[ref_index];
    const uint8_t inc_index = incident_face(inc, ref_normal);
    const Vec2 first = inc.points[inc_index];
    const Vec2 second = inc.points[(inc_index + 1) % inc.count];
    const uint8_t slot = dot(ref_normal, second) < dot(ref_normal, first) ? 1 : 0;
    // Середина между поверхностями — тем же правилом, что в отсечении и на двух других путях.
    const Vec2 point = (slot == 0 ? first : second) -
                       ref_normal * (inc.radius + fix32::from_raw(separation.raw / 2));

    out.normal = flip ? -ref_normal : ref_normal;
    out.count = 1;
    ManifoldPoint& p = out.points[0];
    p = ManifoldPoint{};
    p.anchor_a = point - center_a;
    p.anchor_b = point - center_b;
    p.penetration = -separation;
    p.id = REF_AXIS_ID_BIT | feature_id(flip, ref_index, inc_index, slot);
    // Отказать нечему: зазор опорной грани проверен на неположительность вызывающим, а направление
    // взято у той же грани. Возврат константой — чтобы вызывающий выражал развилку одним `return`.
    return true;
}

} // namespace

bool collide_sat(const WorldShape& a, Vec2 center_a, const WorldShape& b, Vec2 center_b, fix32 margin,
                 Manifold& out) {
    // Форма без ребра сюда попасть не должна: у неё нет ни одной оси для перебора, и `deepest_face`
    // вернул бы начальное значение, то есть «глубочайшее пересечение» из ничего.
    if (a.count < 2 || b.count < 2) return false;

    // Ось с зазором ШИРЕ поля разделяет формы окончательно — по теореме о разделяющей оси этого
    // достаточно, и дальше считать нечего. Внутри поля ось разделяет их лишь пока: контакт там
    // спекулятивный, и весь смысл поля в том, чтобы отдать его решателю ДО перекрытия.
    const FaceQuery qa = deepest_face(a, b);
    if (margin < qa.separation) return false;
    const FaceQuery qb = deepest_face(b, a);
    if (margin < qb.separation) return false;

    // Пересеклись ли САМИ ЯДРА, без скруглений. Ответ точен ровно там, где перебор нормалей ПОЛОН:
    // у пары выпуклых форм разделяющая ось, если она есть, — нормаль чьей-то грани, и множество
    // нормалей полно, пока хотя бы одна форма имеет площадь. Ядро-отрезок своих нормалей не теряет
    // (`sanitize` даёт ему обе, +/- перпендикуляр), поэтому смешанная пара «отрезок об
    // многоугольник» остаётся полной; выпадает только пара ОТРЕЗКОВ — у двух коллинеарных капсул
    // все четыре нормали перпендикулярны общей прямой, оси вдоль неё нет ни одной, и обе проекции
    // совпадают при любом разносе. Там ложное «пересеклись» не стоило бы контакта, а ВЫДУМАЛО бы
    // его: капсулы, разошедшиеся на полюнита при сумме радиусов в восемь, получили бы точку с
    // поперечной нормалью и разъезжались бы боком вместо вдоль.
    const bool cores_meet =
        (a.count > 2 || b.count > 2) &&
        !(fix32{} < max_fix(qa.separation, qb.separation) + a.radius + b.radius);

    const WorldShape* ref = &a;
    const WorldShape* inc = &b;
    uint8_t ref_index = qa.index;
    // Опорной становится грань с БОЛЬШИМ зазором (меньшим проникновением): она и есть та ось, вдоль
    // которой формы разъединяются короче всего, то есть направление, куда решателю их толкать.
    const bool flip = qa.separation + FACE_BIAS < qb.separation;
    if (flip) {
        ref = &b;
        inc = &a;
        ref_index = qb.index;
    }

    if (clip_faces(*ref, ref_index, *inc, flip, center_a, center_b, margin, out)) return true;

    // Ни одной точки при УЖЕ ДОКАЗАННОМ пересечении (обе опорные грани дали неположительный зазор)
    // означает не «контакта нет», а «отсечение его не выразило». Вернуть здесь false значило бы
    // отдать решателю тишину на настоящем перекрытии. Дальше развилка по тому, ЧТО именно
    // пересеклось. Пересеклись ядра — отвечает опорная ось: величины у SAT на руках, и передавать
    // вопрос перебору вершин, который на пересечении слеп, значило бы менять доказанные величины на
    // выдуманные. Ядра разошлись (или полноты перебора нет) — пересеклись скругления, и меряет их
    // расстояние, а не проекции: обоснование целиком в `gap.hpp`.
    const fix32 ref_sep = flip ? qb.separation : qa.separation;
    if (cores_meet) {
        return axis_contact(*ref, ref_index, *inc, ref_sep, flip, center_a, center_b, out);
    }
    return collide_core_gap(a, center_a, b, center_b, margin, out);
}

} // namespace framework::physics
