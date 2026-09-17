#include "manifest.hpp"

#include <cstdio>
#include <string>

#include "platform_args.hpp"
#include "plugin_api.h"

// Гейт конфигурируемого интерфейса (спека #6, гейт 6): данные манифеста → панели и док-слоты.
//
// Второй предмет — ВЕРДИКТ О ВЕРСИИ ABI (аудит #21, A·2·7). Он живёт в том же `parse_manifest`,
// отдаётся тем же `ok`, и отличим от структурного отказа только текстом `error` — то есть
// спрашивать его надо у того же входа и в том же файле, иначе гейт проверял бы одну половину
// вердикта, а вторая уезжала бы в цель, которой никто не зовёт.
//
// Фикстур одиннадцать, а не две: у отказа шесть РАЗНЫХ причин — поля нет, значение не число, цифры не
// влезли в int, число чужое, идентичность объявлена дважды, ключ `api=` повторён, — и каждая
// чинится по-своему. Одна фикстура на все шесть доказывала бы только то, что отказ бывает.
// Десятая — структурная: она стоит за то, что вердикт о версии НЕ перетирает отказ, найденный
// раньше него. Одиннадцатая — про форму: поле написано, но не так, как его читают.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

bool says(const Manifest& m, const std::string& needle) {
    return m.error.find(needle) != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 12) {
        std::fprintf(stderr, "usage: plugin_manifest_test <inspector.manifest> <console.manifest>"
                             " <bad.manifest> <stale.manifest> <no_api.manifest>"
                             " <empty_api.manifest> <overflow.manifest> <dup_plugin.manifest>"
                             " <dup_api.manifest> <no_panels.manifest> <api_typo.manifest>\n");
        return 2;
    }
    const Manifest a = parse_manifest(argv[1]);
    const Manifest b = parse_manifest(argv[2]);
    const Manifest bad = parse_manifest(argv[3]);
    const Manifest stale = parse_manifest(argv[4]);
    const Manifest mute = parse_manifest(argv[5]);
    const Manifest hush = parse_manifest(argv[6]);
    const Manifest huge = parse_manifest(argv[7]);
    const Manifest twin = parse_manifest(argv[8]);
    const Manifest twice = parse_manifest(argv[9]);
    const Manifest lonely = parse_manifest(argv[10]);
    const Manifest typo = parse_manifest(argv[11]);

    std::printf("[manifest] %s: ok=%d id=%s api=%d panels=%zu\n",
                argv[1], a.ok, a.id.c_str(), a.api_version, a.panels.size());
    for (const auto& p : a.panels)
        std::printf("[manifest]   panel '%s' title=\"%s\" dock=%s widgets=%zu\n",
                    p.id.c_str(), p.title.c_str(), dock_name(p.dock), p.widgets.size());
    std::printf("[manifest] %s: ok=%d id=%s panels=%zu\n",
                argv[2], b.ok, b.id.c_str(), b.panels.size());
    // Причины отказа печатаются полностью: расхождение видно без перезапуска, и видно ИМЕННО то,
    // ради чего находка заведена, — что шесть отказов говорят шесть разных вещей. Рядом остаётся
    // прежняя улика гейта 6: панели у отказавшего файла разбор всё-таки СОБРАЛ, и родиться им не
    // даёт ровно `ok` — потребитель, заглянувший в `panels` мимо `ok`, увидел бы их все.
    std::printf("[manifest] refused (panels: bad=%zu stale=%zu no_api=%zu):\n",
                bad.panels.size(), stale.panels.size(), mute.panels.size());
    std::printf("[manifest]   bad=\"%s\"\n[manifest]   stale=\"%s\"\n[manifest]   no_api=\"%s\"\n"
                "[manifest]   empty_api=\"%s\"\n[manifest]   overflow=\"%s\"\n"
                "[manifest]   dup_plugin=\"%s\"\n[manifest]   dup_api=\"%s\"\n"
                "[manifest]   no_panels=\"%s\"\n[manifest]   api_typo=\"%s\"\n",
                bad.error.c_str(), stale.error.c_str(), mute.error.c_str(), hush.error.c_str(),
                huge.error.c_str(), twin.error.c_str(), twice.error.c_str(), lonely.error.c_str(),
                typo.error.c_str());

    check(a.ok && a.id == "inspector" && a.api_version == PLUGIN_API_VERSION
              && a.panels.size() == 2,
          "control: a manifest of the host's own api version is taken whole");
    check(!a.panels.empty() && a.panels[0].id == "inspector" && a.panels[0].title == "Inspector"
              && a.panels[0].dock == DockSlot::Right && a.panels[0].widgets.size() == 4,
          "the first panel is born from data: id, title, dock and four widgets");
    check(a.panels.size() > 1 && a.panels[1].dock == DockSlot::Left,
          "the second panel docks where its own hint says");
    check(b.ok && b.panels.size() == 2 && b.panels[0].dock == DockSlot::Bottom
              && b.panels[1].dock == DockSlot::Center,
          "control: the second manifest is taken whole, bottom and center");

    // Ради этой строки находка и заведена: файл, у которого неверен ровно один байт — цифра
    // после `api=`, — до сих пор разбирался целиком и рождал панели.
    check(!stale.ok, "a manifest declaring a foreign api version is refused");
    check(stale.api_version == 1, "and the field still carries what the file said");
    check(says(stale, "manifest api=1 but")
              && says(stale, "host wants " + std::to_string(PLUGIN_API_VERSION)),
          "and the refusal names both numbers: the file's and the host's");

    check(!mute.ok, "a manifest that declares no api= at all is refused");
    check(says(mute, "no api=") && !says(mute, "host wants"),
          "and the silent file is told apart from the one that names a foreign version");
    check(!says(mute, "though its plugin line says"),
          "and a file that really is silent is not accused of a typo it did not make");

    // Отказ по промаху формы и отказ по молчанию — разные советы: одному дописать поле, другому
    // исправить уже написанное. Один текст на оба посылал бы половину владельцев не туда.
    check(!typo.ok, "a manifest whose api= is written in a form the parser does not read is refused");
    check(says(typo, "no api=") && says(typo, "'API=2'"),
          "and the refusal quotes the token it did find instead of claiming the field is absent");

    check(!bad.ok, "a non-numeric api= is refused");
    check(says(bad, "not a number") && says(bad, "'v99999999999999999999'"),
          "and the refusal quotes what the file actually said, quotes included");

    // Кавычки пинятся здесь, а не на `bad`: у пустого значения кавычки — единственное, что
    // отличает диагностику от фразы, обрывающейся на двоеточии.
    check(!hush.ok, "an api= with no value at all is refused");
    check(says(hush, "not a number") && says(hush, "''"),
          "and the refusal shows the empty value instead of trailing off after the colon");

    // Единственный вход в `catch` вокруг `std::stoi` во всём дереве — ради него эта цель держит
    // `-EHsc` на Windows. Без фикстуры та ветка не исполнялась ни в одном прогоне.
    check(!huge.ok, "digits that do not fit an int are refused");
    check(says(huge, "does not fit") && !says(huge, "is not a number"),
          "and a number too big is not called 'not a number': it is digits, and the advice differs");

    check(twin.api_version != PLUGIN_API_VERSION,
          "premise: the twin file names a version that is NOT the host's, so order is observable");
    check(!twin.ok, "a manifest that names its identity twice is refused");
    check(says(twin, "second plugin line") && !says(twin, "host wants"),
          "and a doubled identity is said BEFORE the version: no line's number can be trusted yet");

    // Файл, который БЕЗ этого правила читается целиком: последний ключ побеждает, и он-то как раз
    // свой. Чужая версия въезжает первым ключом, которого никто уже не прочтёт. Поля версии у него
    // нет намеренно — так эта же строка пинит и разбор строки идентичности: стоит снова снять два
    // позиционных поля до ключей, и `api=1` уедет в `version`, а повтора никто не увидит.
    check(twice.api_version == PLUGIN_API_VERSION,
          "premise: the twice file's LAST api= is the host's own, so without the rule it is taken");
    check(!twice.ok, "a manifest that names api= twice on one line is refused");
    check(says(twice, "repeats api=") && !says(twice, "second plugin line"),
          "and a repeated key is told apart from a repeated line: they are not the same mistake");

    // Отказ, найденный до вердикта о версии, вердиктом не перетирается: у файла без панелей
    // `api=` свой, и согласие по версии не делает пустой манифест годным.
    check(!lonely.ok, "a manifest with an identity but no panels is refused");
    check(says(lonely, "no plugin id or no panels") && !says(lonely, "host wants")
              && !says(lonely, "no api="),
          "and the structural refusal is what it hears, not a word about its api=");

    // Файл, о котором идёт речь, назван в КАЖДОМ отказе: потребитель, который логирует одно
    // `error` (а это и есть печать выше), иначе теряет предмет.
    check(says(stale, argv[4]) && says(mute, argv[5]) && says(hush, argv[6])
              && says(huge, argv[7]) && says(twin, argv[8]) && says(twice, argv[9])
              && says(lonely, argv[10]) && says(typo, argv[11]),
          "every refusal names the file it is talking about");

    // Число, которого разбор не получил, остаётся нулём — и именно поэтому вердикт не смотрит на
    // него: ноль здесь неотличим от нуля у манифеста без поля вовсе.
    check(bad.api_version == 0 && huge.api_version == 0,
          "a number the parser never got stays zero, which is why the verdict reads error instead");

    std::printf("plugin-manifest: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
