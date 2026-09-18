#include "command.hpp"
#include "refusal_fixture.hpp"
#include "scene.hpp"
#include "serialize.hpp"

#include <string>

// Гейт отказа на пути UNDO (аудит #21, A·2·8). Разбор здесь тот же, что у файла сцены, а текст
// другой: снимок — это ТЕЛО одной сущности, без шапки и без строк `E`, и живёт он в памяти, а не
// на диске. Отсюда и свой гейт: причины 1–2 (шапка) обязаны в нём НЕ срабатывать, номер строки
// обязан считаться по снимку, а исход отказа обязан доходить до шины команд — три утверждения,
// которых у гейта файла нет и быть не может.
using namespace ide;
using namespace ide::refusal;

int main() {
    const std::string POS = pos_lines();

    // Путь undo: снимок читается тем же разбором, и отказ на нём обязан быть виден шине.
    Scene undo_scene;
    undo_scene.create(30).set<Position>({fix32::from_int(1), fix32::from_int(2)});
    std::string undo_why;
    // Тело из ДВУХ строк, и сломана ВТОРАЯ: реализация, которая номер не считает вовсе, а печатает
    // единицу, прошла бы однострочную фикстуру насквозь (ревью аудита #21, A·2·8).
    const std::string two_lines = "C Name {\"value\":\"ok\"}\nC Position {\"x\":";
    check(!restore_entity(undo_scene, 40, two_lines, &undo_why),
          "a snapshot that does not parse refuses too");
    // Номер в снимке считается по СНИМКУ: тот же префикс `line ` увёл бы искать правку в
    // произвольное место файла сцены, где строки 1 может не быть вовсе.
    check(says(undo_why, "snapshot line 2"),
          "and a snapshot says which text its line number belongs to, counting its own lines");
    check(!undo_scene.exists(40),
          "and no half-built entity is left behind looking whole");
    check(undo_scene.exists(30), "and the rest of the scene is untouched by that refusal");

    // Шапки у снимка НЕТ и быть не должно: он не файл, а тело одной сущности. Проверка ровно
    // против того, чтобы причины 1–2 уехали в общий код разбора (ревью, A·2·8).
    std::string plain_why;
    check(restore_entity(undo_scene, 60, POS, &plain_why),
          "a snapshot body needs no header of its own");
    check(undo_scene.exists(60), "and the entity it describes is restored");

    // Снимок состоит ТОЛЬКО из строк `C`: строка `E` в нём означает, что тело склеили не с тем
    // текстом, и принять её значило бы восстановить сущность из чужого куска файла. Остальная
    // часть тела здесь ЦЕЛАЯ — иначе отказ приходил бы по ней, и утверждение было бы не о `E`.
    std::string alien_why;
    check(!restore_entity(undo_scene, 50, "E 50\n" + POS, &alien_why),
          "a snapshot body with a line that is not a component refuses");
    check(says(alien_why, "unrecognized line") && !undo_scene.exists(50),
          "and it says so instead of quietly restoring what it could read");

    // Занятый guid переживает битое тело ЦЕЛИКОМ — вместе со значениями (решение владельца).
    // Прежде разбор шёл прямо в сцену: `create` на существующей сущности зовёт `destruct()`, то
    // есть неудачная отмена СНОСИЛА живую сущность и оставляла на её месте огрызок. Утверждение о
    // сериализации, а не о наличии: пережить обязано и содержимое.
    const std::string before30 = serialize_entity(undo_scene, 30);
    std::string occupied_why;
    check(!restore_entity(undo_scene, 30, "C Position {\"x\":", &occupied_why),
          "a snapshot whose body is broken refuses on a guid that is already taken");
    check(undo_scene.exists(30) && serialize_entity(undo_scene, 30) == before30,
          "and the entity that was standing there is byte-identical to before");

    // Тело снимка живёт в памяти, но пишет его СБОРКА, а читать может чужая, и цитата из него
    // уезжает в ту же панель, что и цитата файла. Отказ обязан остаться читаемым и здесь: путь
    // через `quoted` общий, а вот текст, который в него приходит, — свой (ревью, A·2·8).
    std::string raw_why;
    check(!restore_entity(undo_scene, 70, "X \xc3\x28\xff \x1b[31m", &raw_why),
          "a snapshot body of foreign bytes refuses");
    check(valid_utf8(raw_why) && raw_why.find('\x1b') == std::string::npos,
          "and its refusal is printable UTF-8, not the bytes it was handed");

    // Длинное тело из СПЛОШНЫХ продолжающих байтов — вырожденный вход для реза цитаты: откат по
    // границе символа уходит до нуля, и цитата выходила пустой, `'...'`. Причину при этом искать
    // не по чему — то есть ровно та беда, ради которой цитату и санитайзили (ревью, A·2·8).
    std::string tail_why;
    check(!restore_entity(undo_scene, 71, std::string(70, '\x80'), &tail_why),
          "a snapshot body of nothing but continuation bytes refuses");
    check(tail_why.find('?') != std::string::npos,
          "and its quote still shows the bytes as markers, not an empty pair of ticks");

    // Снимок пишет редактор, а читает его в том числе чужая сборка — и на Windows он мог съездить
    // на диск и обратно с `\r` на конце строк. Тело с CRLF — то же тело.
    std::string crlf_body;
    for (char c : POS) { if (c == '\n') crlf_body += '\r'; crlf_body += c; }
    check(crlf_body != POS, "fixture: the snapshot body really carries CR before every LF");
    check(restore_entity(undo_scene, 80, crlf_body), "a snapshot body with CRLF is read back");
    const Position* cp = undo_scene.exists(80) ? undo_scene.get(80).try_get<Position>() : nullptr;
    check(cp && cp->x == fix32::from_int(3) && cp->y == fix32::from_int(5),
          "and its values are the values, not the zeros of a silent refusal");

    // Контроль шины: снимок, который пишет сам редактор, разбирается, undo восстанавливает
    // сущность И ГОВОРИТ ОБ ЭТОМ. Исход прежде выбрасывался: отказ снимка оставлял сцену без
    // сущности, а история шагала вперёд — шина считала шаг отменённым, и redo предлагал повторить
    // то, чего не было (ревью аудита #21, A·2·8).
    CommandBus bus(undo_scene);
    bus.destroy_entity(30);
    check(!undo_scene.exists(30), "control: the bus destroyed the entity");
    check(bus.undo(), "control: an undo of a good snapshot reports that it happened");
    check(undo_scene.exists(30), "control: and the entity is back");
    check(bus.can_redo(), "control: only an undo that happened may be redone");

    // Отмена, которая НЕ состоялась, обязана дойти до шины ЦЕЛИКОМ: и ответом `undo()`, и тем,
    // что шаг не уехал в redo-хвост. Команда собрана здесь руками, а не через `destroy_entity`,
    // по необходимости: снимок, который пишет сам редактор, разбирается ВСЕГДА, и через публичный
    // путь до ложной ветки не дойти вовсе — а утверждать надо именно про неё (решение владельца,
    // A·2·8). Прежде исход отмены выбрасывался, и этот блок краснеет на той реализации.
    CommandBus refusing(undo_scene);
    Command refuses;
    refuses.redo = []() {};
    refuses.undo = []() { return false; };
    refusing.execute(std::move(refuses));
    check(!refusing.undo(), "an undo that did not happen is reported as not having happened");
    check(!refusing.can_redo(), "and a step that was not undone is not offered for redo");
    // Шаг остаётся ЦЕЛЫМ и на месте: транзакция возвращается в `done_`. Прежде отказ терял её
    // вовсе — повторить отмену было уже нечем, и `can_undo()` про неё не знал (решение владельца).
    check(refusing.can_undo(), "and the step that was not undone is still there to be undone");

    // Группа, где отказывает ПОСЛЕДНЯЯ по счёту отмены: три команды до неё уже откатились, и
    // накатить их обратно обязана сама шина. Иначе drag из пятидесяти `set` отменился бы наполовину
    // и остался в таком виде навсегда — состояние, которого пользователь не создавал никогда.
    //
    // Команд именно три, и они именно такие, потому что утверждение ниже ОДНО, а сломаться накат
    // может двояко. Порядок: `redo` у `set_component` пишет значение АБСОЛЮТНО, поэтому накат с
    // конца оставил бы Position=(1,1) вместо (2,2) — две правки одного компонента ловят порядок.
    // Счёт: накат, начатый не с той команды, потерял бы Velocity целиком — компонент, которого до
    // группы не было, ловит длину хвоста. С одной командой в группе не видно ни того, ни другого
    // (ревью аудита #21, A·2·8).
    Scene grp_scene;
    grp_scene.create(90).set<Position>({fix32::from_int(7), fix32::from_int(8)});
    const std::string before90 = serialize_entity(grp_scene, 90);
    CommandBus grp(grp_scene);
    grp.begin_group();
    Command refuses_first;   // отменяется ПОСЛЕДНЕЙ (отмена идёт с конца) — на ней и срыв
    refuses_first.redo = []() {};
    refuses_first.undo = []() { return false; };
    grp.execute(std::move(refuses_first));
    grp.set_component<Velocity>(90, {fix32::from_int(9), fix32::from_int(9)});
    grp.set_component<Position>(90, {fix32::from_int(1), fix32::from_int(1)});
    grp.set_component<Position>(90, {fix32::from_int(2), fix32::from_int(2)});
    grp.end_group();
    check(serialize_entity(grp_scene, 90) != before90, "control: the group changed the scene");
    const std::string after_group = serialize_entity(grp_scene, 90);
    check(!grp.undo(), "an undo whose group refuses part-way reports that it did not happen");
    check(serialize_entity(grp_scene, 90) == after_group,
          "and the commands that had already been undone are rolled forward, in order, byte for byte");
    check(grp.can_undo() && !grp.can_redo(), "and the group is left whole on the undo stack");

    return report("scene-undo-refusal");
}
