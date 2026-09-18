#include "refusal_fixture.hpp"
#include "scene.hpp"
#include "serialize.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

// Гейт отказа загрузчика ФАЙЛА сцены (аудит #21, A·2·8). Предмет отдельный от гейта 1
// (`scene_roundtrip`): тот утверждает, что ЦЕЛЫЙ текст читается байт-в-байт, этот — что БИТЫЙ не
// читается вовсе. Отдельный и от `scene_undo_refusal_test`: там тот же разбор читает ТЕЛО СНИМКА,
// у которого нет ни шапки, ни строк `E`, и общий гейт на два текста не сказал бы, который из них
// сломался.
//
// Причин отказа четырнадцать, их список и порядок — в `serialize.hpp`, одним местом на весь гейт.
// Каждая чинится по-своему, поэтому у каждой своя фикстура: одна фикстура на все четырнадцать
// доказывала бы только то, что отказ бывает. А проверять надо ровно обратное тому, что было, —
// раньше КАЖДАЯ из них молча пропускалась, и сцена собиралась из того, что уцелело.
using namespace ide;
using namespace ide::refusal;

namespace {

// Фикстура случая — ЦЕЛЫЙ текст, а не только битая строка: так видно, что отказ роняет весь
// разбор, а не только свою строку. `shown` отдельно от `text` затем, что в лог уезжает короткая
// приметa случая, а не фикстура целиком.
struct Case {
    std::string text;
    std::string shown;
    const char* needle;
    const char* what;
};

} // namespace

int main() {
    const std::string HEAD = head_text();
    const std::string POS = pos_lines();
    // Одна строка `C Position <json>` без префикса — канонический json позиции (3,5) из первых
    // рук: переписать его сюда значило бы пинить свою память о формате, а не формат.
    const size_t js = POS.find(' ', 2) + 1;
    const std::string POS_JSON = POS.substr(js, POS.find('\n') - js);
    auto with = [&HEAD](const std::string& bad) { return HEAD + bad + "\n"; };

    // Контроль: целый текст читается, `why` при этом молчит, а прочитанные ЗНАЧЕНИЯ те самые.
    // Без проверки значений все отказы ниже прошли бы и на загрузчике, который отказывает ВСЕГДА,
    // и — что тоньше — на сверке с каноном, отвергающей всё подряд.
    std::string why = "not touched";
    Scene ok_scene;
    const std::string whole = HEAD + "E 20\n" + POS;
    check(deserialize(ok_scene, whole, &why), "control: a whole scene text is taken");
    check(why.empty(), "and a load that succeeded leaves no reason behind");
    check(ok_scene.exists(10) && ok_scene.exists(20), "control: both entities are born");
    const Name* nm = ok_scene.exists(10) ? ok_scene.get(10).try_get<Name>() : nullptr;
    check(nm && nm->value == "hero",
          "control: a value read from the file is the value in the file");
    const Position* ps = ok_scene.exists(20) ? ok_scene.get(20).try_get<Position>() : nullptr;
    check(ps && ps->x == fix32::from_int(3) && ps->y == fix32::from_int(5),
          "control: the canonical json the writer emits is accepted, member for member");

    const std::vector<Case> cases = {
        {with("E abc"), "E abc", "entity id is not a number",
         "an entity id that is not a number is refused"},
        {with("E 20abc"), "E 20abc", "entity id is not a number",
         "digits followed by letters are refused, not silently read as 20"},
        // Все символы цифры — значит проверку «не число» этот файл проходит насквозь, и причина у
        // него СВОЯ (решение владельца): `E 007` читался как 7, пересохранение писало `E 7`, то
        // есть два текста одного номера — тот же дубль, что ниже, только невидимый глазу.
        {with("E 007"), "E 007", "entity id has a leading zero",
         "an entity id with a leading zero is refused, not renumbered on the next save"},
        // Тоже все цифры, то есть проверку «не число» этот файл проходит насквозь. Раньше разбор
        // насыщался до ULLONG_MAX, и сохранение записывало номер, которого в файле нет.
        {with("E 99999999999999999999999"), "E 999...", "entity id does not fit",
         "an entity id too big for the type is refused"},
        // Номер 10 уже занят шапкой. `Scene::create` зовёт `destruct()` на существующей сущности —
        // то есть второй `E 10` СНОСИЛ первый блок вместе с его компонентами, без единого слова.
        {with("E 10"), "E 10 (dup)", "duplicate entity id",
         "an entity id used twice is refused, not silently collapsed onto the first block"},
        {with("C Position"), "C Position", "component line has no value",
         "a component line with no value is refused"},
        {with("C  {\"x\":1}"), "C  {x:1}", "component line has no name",
         "a component line whose name is empty says so, and quotes the line"},
        // Имя компонента дважды у ОДНОЙ сущности: `Name` у сущности 10 уже задан шапкой, и второй
        // `set<Name>` молча перетирал первый — файл грузился как файл со второй строкой, а
        // пересохранение писало одну (решение владельца, третий заход ревью A·2·8).
        {with("C Name {\"value\":\"other\"}"), "C Name (dup)", "duplicate component",
         "the same component twice on one entity is refused, not silently overwritten"},
        {with("C Wat {\"x\":1}"), "C Wat {x:1}", "unknown component",
         "a component name the build does not know is refused"},
        {with("C Position {\"x\":"), "C Position {x:", "bad json",
         "a value the reflection cannot read is refused"},
        // Json целый, член чужой: `from_json` возвращает не-NULL, `Name` остаётся пустым. Ровно
        // этот тихий ноль — предмет находки: пустое имя, неотличимое от прочитанного.
        {with("E 20\nC Name {\"n\":\"hero\"}"), "C Name {n:hero}",
         "value is not what the format writes",
         "a member the format does not have is refused, not read as an empty value"},
        // Хвост после json: `from_json` возвращает указатель на остаток текста, и остаток молча
        // выбрасывался — файл с приписанным мусором грузился как чистый.
        {with("C Position " + POS_JSON + " lolwut"), "C Position <ok> lolwut",
         "value is not what the format writes",
         "text left over after the json is refused, not dropped on the floor"},
        {with("garbage"), "garbage", "unrecognized line",
         "a line that is not in the format at all is refused"},
        // Шапка обязана быть первой непустой строкой и ровно этой: раньше `#` был просто
        // комментарием, и чужой файл грузился «успешно» пустой сценой, а первое сохранение
        // затирало его текстом сцены.
        {"#!/bin/sh\nE 10\n", "#!/bin/sh", "not a scene file",
         "a file that is not a scene at all is refused before a single entity is born"},
        // Порода та, версия чужая — СВОЯ причина: чужой файл чинят выбором другого файла, файл
        // будущей версии — другой сборкой.
        {"# like-nes scene v2\nE 10\n", "# ... v2", "unsupported format version",
         "a scene file from another format version is refused, and told apart from a foreign file"},
    };

    // Имя случая склеивается в КАЖДОЕ утверждение цикла: красная строка обязана говорить, какая
    // из причин сломалась, иначе по логу видно только, что сломалась одна (ревью, A·2·8).
    for (const Case& c : cases) {
        Scene bad;
        std::string reason;
        const bool taken = deserialize(bad, c.text, &reason);
        std::printf("[refusal] %-26s -> %s\n", c.shown.c_str(), reason.c_str());
        const std::string tag = std::string(" [") + c.needle + "]";
        check(!taken, c.what);
        check(says(reason, c.needle), ("the refusal says which cause it is" + tag).c_str());
        // Ради этой строки находка и заведена: раньше битая строка пропускалась, а сущность 10 из
        // ЦЕЛОЙ части файла оставалась в сцене — то есть редактор показывал полусцену как целую.
        check(!bad.exists(10), ("the scene is left empty, not half-built" + tag).c_str());
        // Не просто подстрока `line `: за ней обязано стоять ЧИСЛО. Иначе утверждение держалось бы
        // и на диагностике, где номер потерялся, а слово осталось (ревью аудита #21, A·2·8).
        const size_t ln = reason.find("line ");
        check(ln != std::string::npos && std::isdigit(static_cast<unsigned char>(reason[ln + 5])),
              ("the refusal names the line to fix" + tag).c_str());
    }

    // Пустой файл шапки не объявил, а значит и сценой себя не назвал. Стоит отдельно от таблицы:
    // сущности 10 в нём нет по построению, и общее утверждение «сцена пуста» было бы вакуумным.
    Scene empty_scene;
    std::string empty_why;
    check(!deserialize(empty_scene, "", &empty_why), "an empty file is not a scene");
    check(says(empty_why, "not a scene file"),
          "and it says so instead of loading as an empty scene");

    // Компонент до первой `E` — свой текст, потому что его битая строка обязана стоять СРАЗУ за
    // шапкой: в `with()` перед ней уже есть сущность.
    Scene orphan;
    std::string orphan_why;
    check(!deserialize(orphan, HEAD.substr(0, HEAD.find('\n') + 1) + "C Name {\"value\":\"a\"}\n",
                       &orphan_why),
          "a component before any entity is refused");
    check(says(orphan_why, "component before any entity") && says(orphan_why, "line 2"),
          "and it is told apart from a component whose entity exists");

    // Решение владельца: отказ не трогает сцену вызывающего ВОВСЕ. Прежний разбор звал `clear()`
    // до первой проверки — то есть открытый документ погибал от чужого файла, выбранного по
    // ошибке. Утверждение о СЕРИАЛИЗАЦИИ, а не о наличии сущности: пережить обязано и значение.
    Scene live;
    check(deserialize(live, whole), "control: the scene to be spared loads first");
    const std::string before = serialize(live);
    check(!deserialize(live, with("garbage")),
          "a bad text refuses on a scene that already has one");
    check(serialize(live) == before, "and the scene it was given back is byte-identical to before");

    // Сверка с каноном сравнивает ТЕКСТЫ, и без этих двух контролей она требовала бы от файла
    // побайтового совпадения с тем, что напечатал flecs. Пробелы вне литералов не значат ничего:
    // писатель ставит их после запятой, правленный руками файл обычно не ставит, и отказ за это
    // был бы отказом за форматирование, а не за содержание (решение владельца, A·2·8).
    Scene tightly;
    std::string tight = POS_JSON;
    tight.erase(std::remove(tight.begin(), tight.end(), ' '), tight.end());
    check(tight != POS_JSON, "fixture: the writer's spacing is what is being dropped here");
    check(deserialize(tightly, HEAD + "E 20\nC Position " + tight + "\n"),
          "a file written without the writer's spacing is still taken");
    const Position* tp = tightly.exists(20) ? tightly.get(20).try_get<Position>() : nullptr;
    check(tp && tp->x == fix32::from_int(3) && tp->y == fix32::from_int(5),
          "and its values are the values, not the zeros of a silent refusal");

    // Вычерк пробелов ОГУЛЬНЫЙ, и утверждение здесь — что огульным он быть и МОЖЕТ: сверка
    // вычёркивает пробел литерала с ОБЕИХ сторон, а само ЗНАЧЕНИЕ идёт из `from_json`, которого
    // вычерк не касается. Текст пишет сериализатор, а не моя память о кавычках.
    Scene spacey;
    Scene src;
    src.create(70).set<Name>({"two words"});
    check(deserialize(spacey, serialize(src)), "a name with a space inside it loads");
    const Name* sn = spacey.exists(70) ? spacey.get(70).try_get<Name>() : nullptr;
    check(sn && sn->value == "two words", "and the space inside the value survives the check");

    // Сущность и компонент названы в диагностике вместе с номером строки: три координаты, по
    // которым правку ищут в файле. Без сущности номер строки ведёт в правильное место, но не
    // говорит, ЧЕЙ это компонент, — а в сейве редактора строк тысячи.
    Scene named;
    std::string named_why;
    (void)deserialize(named, with("C Position {\"x\":"), &named_why);
    check(says(named_why, "line 4") && says(named_why, "entity 10")
              && says(named_why, "component 'Position'"),
          "a refusal names the line, the entity and the component");

    // Длинная строка обрезается, и рез ложится на ГРАНИЦУ символа: здесь 64-й байт — второй байт
    // кириллической буквы, то есть наивная обрезка по длине оставила бы в конце половину символа.
    // Своей причины отказа у этого случая нет — он про то, КАК причина написана, и потому стоит
    // отдельно от таблицы (ревью аудита #21, A·2·8).
    Scene cut;
    std::string cut_why;
    const std::string long_line = std::string(63, 'z') + "\xd1\x8f";
    (void)deserialize(cut, with(long_line), &cut_why);
    std::printf("[refusal] long line -> %s\n", cut_why.c_str());
    check(says(cut_why, "..."), "a line too long to quote whole is cut short");
    check(valid_utf8(cut_why), "and the cut lands on a character boundary, not inside a character");
    // Отдельно от `valid_utf8`: замена битого байта на `?` тоже даёт валидный UTF-8, то есть без
    // этой строки рез по границе стал бы неотличим от реза по длине с последующей заменой. Строка
    // файла тут ЦЕЛАЯ, значит и вопросительным знакам в цитате взяться неоткуда (ревью, A·2·8).
    check(cut_why.find('?') == std::string::npos,
          "and a whole character is kept, not swapped for a marker the file never had");

    // Байты строки диктует тот, кто прислал файл, и UTF-8 среди них не гарантирован ничем: сейв
    // чужого редактора в latin-1, файл, обрезанный на полпути, просто файл не той породы. Такая
    // цитата рвала кодировку ВСЕЙ строки, куда её вставили: панель показывала кашу вместо причины
    // отказа. А байт ESC уезжал в терминал командой, а не символом — чужой файл красил чужой лог.
    // Своей причины отказа у случая нет, он про то, КАК причина написана (ревью аудита #21, A·2·8).
    Scene raw;
    std::string raw_why;
    // Байты подобраны по одному на каждый способ соврать про UTF-8: одиночные `FF FE`, продолжающий
    // байт без начала (`80`), управляющий ESC, пересортица (`C0 80` вместо нуля), суррогат
    // (`ED A0 80`) и оборванный на конце двухбайтовый `C3`. Пересортица и суррогат нужны отдельно:
    // проверка «по старшим битам» пропустила бы их обоих, а декодер панели разберёт их иначе.
    const std::string raw_line = "E \xff\xfe\x80 \x1b[31m \xc0\x80 \xed\xa0\x80 red\xc3";
    check(!deserialize(raw, with(raw_line), &raw_why), "a line of foreign bytes is refused");
    std::printf("[refusal] raw bytes -> %s\n", raw_why.c_str());
    check(valid_utf8(raw_why),
          "and the refusal is valid UTF-8, not the bytes the file handed over");
    check(raw_why.find('\x1b') == std::string::npos,
          "and no escape byte from the file reaches the terminal as a command");

    // Файл, сохранённый редактором Windows, — ТОТ ЖЕ файл: `\r` на конце каждой строки съедается
    // разбором. Без этой фикстуры строка `# like-nes scene v1\r` не равнялась бы шапке, и сейв
    // владельца, открытый once на Windows, отказывался бы как «не файл сцены» (ревью A·2·8).
    Scene crlf;
    std::string crlf_text;
    for (char c : whole) { if (c == '\n') crlf_text += '\r'; crlf_text += c; }
    check(crlf_text != whole, "fixture: the text really carries CR before every LF");
    std::string crlf_why = "not touched";
    check(deserialize(crlf, crlf_text, &crlf_why), "a scene file with CRLF line ends is taken");
    const Name* cn = crlf.exists(10) ? crlf.get(10).try_get<Name>() : nullptr;
    check(cn && cn->value == "hero", "and its values are not left with a stray carriage return");

    return report("scene-refusal");
}
