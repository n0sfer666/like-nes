// Гейт 9 (спека #21, строка 110): устойчивость десериализаторов к битому вводу. Прогон один и тот
// же на трёх ОС и на машине владельца — seed'ы фиксированы в коде, а не выведены из sha коммита:
// мерцающий гейт нарушает то самое требование, ради которого писался свой мутатор, потому что
// красный раннер обязан воспроизводиться одной командой.
//
//     ./fuzz_readers                 — прогон по таблице
//     ./fuzz_readers <seed>          — повтор одного seed'а руками
//     ./fuzz_readers --cases <n>     — та же таблица и те же seed'ы, меньшая глубина
//     ./fuzz_readers --target <имя>  — повтор одной цели; буферы те же, что в общем прогоне
//     ./fuzz_readers --selftest      — ТОЛЬКО дырявая цель; обязана упасть под ASan
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "fuzz_run.hpp"
#include "fuzz_target.hpp"
#include "fuzz_watchdog.hpp"
#include "platform_args.hpp"

namespace {

// Восемь seed'ов, зашитых числами. Набор РАЗНОРОДЕН нарочно: единица, константа golden ratio из
// splitmix64 и её удвоение, два узора полубайтов и два известных множителя LCG/PCG. Один закон
// порождения дал бы восемь соседних точек, а список с чьей-то машины завтра не воспроизвести.
// Расширение поиска — дописать число сюда коммитом, а не отдать выбор раннеру.
constexpr uint64_t SEEDS[] = {
    1ull,
    0x9e3779b97f4a7c15ull,
    0x3c6ef372fe94f82aull,
    0xdaa66d2b71a12917ull,
    0x0123456789abcdefull,
    0xfedcba9876543210ull,
    0x5851f42d4c957f2dull,
    0x2545f4914f6cdd1dull,
};
constexpr int DEFAULT_CASES = 2000;

// Нижний порог ВЫБОРКИ — произведения `cases × seeds`, а не одной глубины. Сторож вакуума («ни
// один изменённый буфер не прошёл конверт») судит о ФОРМАТЕ, а на мелкой выборке он судит о
// выборке: у самой узкой цели (`ide-scene` — 168 принятых из 16000) мимо конверта проходят единицы
// случаев из тысячи, и `--cases 5` давал отказ на заведомо здоровом дереве. Порог на одной глубине
// отбивал бы не ту величину, о которой судит сторож: `fuzz_readers 1 --cases 200` оставляет один
// seed, выборка падает в восемь раз, и та же цель краснеет на здоровом дереве. 1600 — выборка
// самой медленной задачи CI (ASan под clang-cl: восемь зашитых seed'ов на `--cases 200`). Пол
// пропускает и `fuzz_readers <seed>` (2000 × 1), поэтому замерена и она: у самой тонкой цели там
// 20 принятых. Обе пропускаемые конфигурации доказаны прогоном, а не рассуждением.
constexpr long long MIN_SAMPLE = 1600;

// Сторож опрашивает дедлайн десять раз в секунду: задержка обнаружения ниже десятой доли
// секунды при прогоне в десятки секунд, а стоимость — спящий поток, просыпающийся 330 раз за
// весь гейт.
constexpr double WATCHDOG_POLL_SEC = 0.1;

int usage() {
    std::fprintf(stderr,
                 "usage: fuzz_readers [<seed>] [--cases <n>] [--target <name>] [--selftest]\n"
                 "       --cases <n>: 1..1000000, and cases x seeds must be >= %lld; below that\n"
                 "       floor the vacuum check would judge the sample size, not the format\n",
                 MIN_SAMPLE);
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    // Буферизация stdout снята ВОВСЕ. Ни один документированный вызов гейта не пишет в терминал:
    // три шага CI пайпят (`| tee fuzz.txt`), два перенаправляют в файл (`> fuzz_self.txt`),
    // `preflight_build_rules.sh` читает `out=$(…)` — и в каждом из этих случаев libc буферизует
    // ПОЛНОСТЬЮ. Десяток строк `[fuzz] <цель>: cases=… accepts=…` остался бы тогда в буфере на
    // КАЖДОМ аварийном выходе: `_Exit` сторожа, `_Exit` цели плагина и `abort()` самого санитайзера
    // на настоящей находке — то есть терялось бы ровно то, что называет, какие цели уже прошли и на
    // какой встал прогон.
    //
    // Режим именно `_IONBF`, а не `_IOLBF`: UCRT документирует `_IOLBF` как «то же, что `_IOFBF`»,
    // то есть на Windows построчного режима нет вовсе, а `_IONBF` буфера не заводит и размер
    // игнорирует на всех трёх ОС. Это единственный способ закрыть чужой `abort()` санитайзера, под
    // который своего сброса не подложить; цена нулевая — строк вывода у гейта десяток. Возврат не
    // проверяется нарочно: отказ оставил бы полную буферизацию, а её последствия уже закрыты явным
    // `fflush(stdout)` после каждой строки вердикта и перед каждым `_Exit`.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // Шов argv: на Windows узкий argv из CRT приезжает в ANSI, и seed, поданный руками, ещё
    // можно прочитать, а путь — уже нет. Берём тот же шов, что все тридцать целей дерева.
    platform::Args utf8_argv(argc, argv);
    std::vector<uint64_t> seeds(SEEDS, SEEDS + sizeof(SEEDS) / sizeof(SEEDS[0]));
    int cases = DEFAULT_CASES;
    bool selftest = false;
    std::string only;

    bool seed_given = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--selftest") {
            selftest = true;
        } else if (a == "--cases") {
            // Глубина — ИМЕНОВАННЫМ флагом, а не вторым позиционным: вторым позиционным её не
            // урезать, не сузив заодно список seed'ов до одного, а на самой медленной задаче CI
            // (ASan под clang-cl) резать надо ровно глубину. Восемь seed'ов там и есть предмет:
            // они зашиты в код именно для того, чтобы все конфигурации смотрели в одни места.
            if (++i >= argc) return usage();
            // `strtol`, а не `atoi`: у `atoi` выход за диапазон `int` — неопределённое поведение,
            // и страж `<= 0` его не ловит. Опечатка в числе уводила прогон в многочасовой цикл,
            // неотличимый от зависания, — то есть гейт ломался ровно тем способом, ради которого
            // писался. Потолок здесь же: всё выше — заведомо опечатка, а не намерение.
            errno = 0;
            char* end = nullptr;
            const long n = std::strtol(argv[i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' || n < 1 || n > 1000000) {
                return usage();
            }
            cases = static_cast<int>(n);
        } else if (a == "--target") {
            // Повтор ОДНОЙ цели: упавшую не переспросить, не прогнав заодно девять соседних, а под
            // санитайзером это разница в минуты. Номер цели в ключе случая остаётся табличным (см.
            // ниже), поэтому буферы совпадают с теми, что были в общем прогоне.
            // Пустое имя отбивается здесь: иначе `only` остаётся пустой строкой, ниже она
            // неотличима от «`--target` не задан», и прогон молча вырождается в ПОЛНЫЙ. Страж
            // `ran == 0`, поставленный ровно затем, чтобы опечатка в имени была громкой, такой
            // опечатки не увидит — оператор просил одну цель, а ждёт десять.
            if (++i >= argc || argv[i][0] == '\0') return usage();
            only = argv[i];
        } else if (!a.empty() && a[0] == '-') {
            return usage();
        } else if (!seed_given) {
            // Разбор тот же, что у `--cases`, и по той же причине: `strtoull` без проверки принимал
            // `fuzz_readers abc` за seed 0 и молча гонял НЕ ту выборку, о которой просили, а строка
            // PASS называла чужое число — повтор упавшего раннера повторял бы не его. Основание 0:
            // seed'ы таблицы записаны шестнадцатерично, и руками их переносят в том же виде.
            errno = 0;
            char* end = nullptr;
            const unsigned long long one_seed = std::strtoull(a.c_str(), &end, 0);
            if (errno != 0 || end == a.c_str() || *end != '\0') return usage();
            seeds.assign(1, one_seed);
            seed_given = true;
        } else {
            // Второй seed молча затирал бы первый: прогон назвал бы в PASS не то, чем гонялся.
            return usage();
        }
    }

    // Порог считается ПОСЛЕ разбора: величина, о которой судит сторож вакуума, известна только
    // когда известны обе — и глубина, и число seed'ов, а они приходят разными аргументами.
    const long long sample = static_cast<long long>(cases) * static_cast<long long>(seeds.size());
    if (sample < MIN_SAMPLE) {
        std::fprintf(stderr, "[fuzz] FAIL: sample %lld = %d cases x %zu seeds, floor is %lld\n",
                     sample, cases, seeds.size(), MIN_SAMPLE);
        return usage();
    }

    fuzz::watchdog::start(WATCHDOG_POLL_SEC);

    fuzz::Stats st;
    if (selftest) {
        // Ожидание здесь ОБРАТНОЕ, и проверить его изнутри нечем: цель обязана упасть, а упавший
        // процесс вердикта не печатает. Код возврата тут 1 в ОБОИХ исходах — зелёного вердикта у
        // самопроверки не бывает, — поэтому судит шаг CI по СТРОКЕ ниже: она печатается ровно в том
        // случае, когда гейт НЕ работает (см. `.github/workflows/ci.yml`, обе задачи с санитайзером).
        const fuzz::Target& c = fuzz::canary();
        // «Выжила» печатается, только если цель дошла до конца. Свой отказ `run_target` уже
        // назвала сама, и приписывать ей выживание было бы ВТОРЫМ текстом про первое событие:
        // упавший по порогу прогон выглядел бы как работающий гейт.
        if (fuzz::run_target(c, 0, seeds.data(), seeds.size(), cases, &st)) {
            std::fprintf(stderr, "[fuzz] FAIL selftest: the canary survived %lld cases\n",
                         st.cases);
        }
        return 1;
    }

    std::size_t count = 0;
    const fuzz::Target* table = fuzz::targets(&count);
    std::size_t ran = 0;
    for (std::size_t t = 0; t < count; ++t) {
        if (!only.empty() && only != table[t].name) continue;
        // Счёт ведётся на КАЖДУЮ цель отдельно, а общий складывается из них: одно глобальное
        // число не даёт увидеть цель, ушедшую в вакуум, — её ноль тонет в чужих тысячах.
        // Номер цели для ключа случая — табличный `t`, а не порядок в этом прогоне: иначе
        // `--target` смотрел бы на другие буферы, чем общий прогон, и повтор ничего не повторял.
        fuzz::Stats one;
        if (!fuzz::run_target(table[t], t, seeds.data(), seeds.size(), cases, &one)) return 1;
        std::printf("[fuzz] %s: cases=%lld accepts=%lld\n", table[t].name, one.cases, one.accepts);
        std::fflush(stdout);  // вердикт цели обязан пережить аварийный выход следующей
        st.cases += one.cases;
        st.accepts += one.accepts;
        ++ran;
    }
    if (ran == 0) {
        // Опечатка в имени цели молча дала бы пустой зелёный прогон — тот самый вакуум, против
        // которого стоит проверка выше.
        std::fprintf(stderr, "[fuzz] FAIL: no target named %s\n", only.c_str());
        return 1;
    }
    std::printf("[fuzz] PASS targets=%zu seeds=%zu cases=%lld accepts=%lld\n", ran, seeds.size(),
                st.cases, st.accepts);
    return 0;
}
