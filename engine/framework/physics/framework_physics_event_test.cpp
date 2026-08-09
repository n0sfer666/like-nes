#include <cstdio>

#include "platform_args.hpp"
#include "world.hpp"

// События контакта: фазы во ВРЕМЕНИ (одно начало, один конец, продолжение между ними) и порядок
// ВНУТРИ кадра (по ключу пары). Сцена всюду кинематическая: тело едет с заданной скоростью и на
// удары не отвечает, поэтому расписание фаз — функция раскладки, а не того, насколько сильно
// решатель успел его оттолкнуть.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::physics;

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);

BodyDesc mover(uint32_t key) {
    BodyDesc d;
    d.key = key;
    d.type = BodyType::Kinematic;
    d.shape = box(fix32::from_int(4), fix32::from_int(4));
    d.position = {fix32::from_float(-20.5), fix32{}};
    d.velocity = {fix32::from_int(60), fix32{}};
    return d;
}

BodyDesc zone(uint32_t key, fix32 x) {
    BodyDesc d;
    d.key = key;
    d.type = BodyType::Static;
    d.trigger = true;
    d.shape = box(fix32::from_int(8), fix32::from_int(8));
    d.position = {x, fix32{}};
    return d;
}

// Тело шириной 8 и зона шириной 16 в нуле перекрываются на |x| < 12; тело едет по юниту за шаг с
// -20.5, значит проезд занимает около двадцати четырёх шагов. Сорока хватает на весь путь с запасом.
void test_phase_schedule() {
    World w(8);
    w.add(mover(1));
    w.add(zone(2, fix32{}));

    uint32_t begins = 0;
    uint32_t stays = 0;
    uint32_t ends = 0;
    uint32_t first_begin = 0;
    uint32_t last_stay = 0;
    uint32_t end_frame = 0;
    bool depth_seen = false;
    bool end_payload_empty = true;
    bool always_trigger = true;
    bool point_off_overlap = false;

    for (uint32_t frame = 1; frame <= 40; ++frame) {
        w.step(DT);
        check(w.contact_count() == 0, "a trigger pair never becomes a solved contact");
        for (const ContactEvent& e : w.events()) {
            if (!e.trigger) always_trigger = false;
            switch (e.phase) {
            case ContactPhase::Begin:
                ++begins;
                first_begin = frame;
                if (e.penetration.raw > 0) depth_seen = true;
                // Точка обязана лежать В ПЕРЕКРЫТИИ на момент ОБНАРУЖЕНИЯ: на первом касании (тело
                // на -11.5) это полоса от -8 до -7.5, уже пути за кадр, и точка, посчитанная от
                // позиции ПОСЛЕ интеграции, из неё выпадает — так и было до фикса ревью PR B.
                if (e.point.x < fix32::from_int(-8) || fix32::from_float(-7.5) < e.point.x) {
                    point_off_overlap = true;
                }
                break;
            case ContactPhase::Stay:
                ++stays;
                last_stay = frame;
                if (e.penetration.raw > 0) depth_seen = true;
                break;
            case ContactPhase::End:
                ++ends;
                end_frame = frame;
                if (e.penetration.raw != 0 || e.point.x.raw != 0 || e.normal.x.raw != 0) {
                    end_payload_empty = false;
                }
                break;
            }
        }
    }

    check(begins == 1, "crossing a zone begins exactly once");
    check(ends == 1, "and ends exactly once");
    check(stays > 10, "with the crossing itself reported as stay frames");
    check(first_begin < last_stay && last_stay < end_frame, "in that order in time");
    check(depth_seen, "and the touching events carry a real depth, not a bare flag");
    check(!point_off_overlap, "with the contact point inside the overlap at the detected position");
    check(end_payload_empty, "while the end event carries identity only");
    check(always_trigger, "every event of this pair is marked as a trigger event");

    // Тело проехало зону насквозь и не потеряло скорости: триггер не толкает. Утверждение стоит
    // здесь, а не в фильтре, потому что проверяет ВЕСЬ проезд, а не один кадр перекрытия.
    check(w.bodies()[0].velocity.x == fix32::from_int(60), "and the mover is never pushed");
    check(fix32::from_int(15) < w.bodies()[0].position.x, "having driven clean through");
}

// Две зоны разом, созданные в порядке, обратном ключам: порядок событий обязан задаваться ключом.
void test_frame_order() {
    World w(8);
    w.add(mover(1));
    w.add(zone(5, fix32::from_int(-2)));
    w.add(zone(3, fix32::from_int(2)));

    for (uint32_t frame = 0; frame < 40; ++frame) {
        w.step(DT);
        if (w.events().size() < 2) continue;
        check(w.events()[0].key_b == 3 && w.events()[1].key_b == 5,
              "events inside a frame come by ascending pair key, not creation order");
        return;
    }
    check(false, "the mover was expected to overlap both zones on some frame");
}

// Тело меняет роль на ходу: зона перестаёт быть зоной. Это КОНЕЦ одного касания и НАЧАЛО другого в
// одном кадре, а не продолжение: у игры на зону и на стену разные обработчики, и «stay» с новым
// флагом дошёл бы до обработчика, который начала не видел.
void test_role_change() {
    World w(8);
    // Тело здесь ДИНАМИЧЕСКОЕ, в отличие от сцен выше, и это не косметика: пара «кинематика против
    // статики» без флага триггера отбрасывается широкой фазой законно — импульс не сдвинет ни одну
    // сторону. Такое тело дало бы после переключения роли один только `end`, и утверждение ниже
    // проверяло бы отсечку широкой фазы вместо смены роли.
    w.set_gravity({fix32{}, fix32{}});
    BodyDesc d = mover(1);
    d.type = BodyType::Dynamic;
    w.add(d);
    w.add(zone(2, fix32{}));

    for (uint32_t frame = 0; frame < 40; ++frame) {
        w.step(DT);
        if (w.events().empty() || w.events()[0].phase != ContactPhase::Stay) continue;

        w.body(BodyId{1}).trigger = false;
        w.step(DT);
        check(w.events().size() == 2, "flipping the role splits the frame into two events");
        if (w.events().size() != 2) return;
        check(w.events()[0].phase == ContactPhase::End && w.events()[0].trigger,
              "the trigger touch ends first");
        check(w.events()[1].phase == ContactPhase::Begin && !w.events()[1].trigger,
              "and the solid touch begins after it");
        check(w.contact_count() == 1, "and the pair is now solved as a contact");
        return;
    }
    check(false, "the mover was expected to settle into stay frames");
}

// Голден потока событий. Число снято с прогона, на котором прошли ВСЕ утверждения выше: иначе оно
// пинит не поведение, а то, что код делал в момент снятия. Ровно так и вышло на первом прогоне PR B:
// значение сняли с кода, собиравшего события в конце шага, и оно закрепило точку, отнесённую от
// места удара на путь за кадр. Ловит такое не голден, а утверждение о полосе перекрытия: до фикса
// оно красное, пока голден зелен.
constexpr uint64_t EVENT_GOLDEN = 0x2c191ca82be0defcULL;

void test_golden() {
    World w(16);
    w.add(mover(1));
    w.add(zone(5, fix32::from_int(-2)));
    w.add(zone(3, fix32::from_int(2)));
    for (uint32_t frame = 0; frame < 40; ++frame) w.step(DT);
    const uint64_t got = w.event_hash();
    std::printf("  event hash: 0x%016llx\n", static_cast<unsigned long long>(got));
    check(got == EVENT_GOLDEN, "the event stream matches the pinned golden");

    // Позитивный контроль самой свёртки: сцена без единого события обязана дать ДРУГОЕ число.
    // Свёртка, которая молча ничего не мешает, совпала бы с голденом на любой сцене — и голден
    // проверял бы, что накопитель существует, а не что поток событий тот же.
    World empty(16);
    empty.add(zone(9, fix32::from_int(1000)));
    for (uint32_t frame = 0; frame < 40; ++frame) empty.step(DT);
    check(empty.event_hash() != got, "an eventless run folds to a different value");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics contact events gate\n");
    test_phase_schedule();
    test_frame_order();
    test_role_change();
    test_golden();
    std::printf("framework-physics-event: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
