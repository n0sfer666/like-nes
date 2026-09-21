// Гейт границы C-ABI (аудит #21, находка A·2·10). Зовёт входы `HostApi` так, как их зовёт
// сломанный или злой плагин: nullptr вместо строки, отрицательный и запредельный счётчик
// зависимостей, дыра посреди массива `after`. Утверждается не «не упало», а ЧТО РЕЕСТР НЕ ВЫРОС:
// запись с пустым обработчиком дожила бы до тика или до декодирования и упала бы там, где по логу
// виноват движок, а не тот, кто её зарегистрировал.
//
// Тест идёт и под ASan/UBSan (шаг «Plugin — ASan/UBSan»), и это не украшение: массив зависимостей
// здесь ровно `PLUGIN_MAX_SYSTEM_DEPS` длиной, а счётчиком передаётся на единицу больше — снятый
// потолок читает мимо конца, и ассерту такое чтение НЕ видно, а санитайзеру видно.
#include "registry.hpp"
#include "platform_args.hpp"
#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[plugin-abi] FAIL: %s\n", what);
    ++failures;
}

static void sys_noop(SimWorld*) {}
static int32_t decode_noop(const uint8_t*, int32_t, uint8_t*, int32_t) { return 0; }
static void draw_noop(void*) {}
static int32_t unlock_noop(void*, const char*) { return ACH_SEND_OK; }

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);

    // Счётчик расширений до вызова и после — единственный способ отличить «отказано» от
    // «принято и промолчало»: танки возвращают void, потому что так устроен C-ABI плагина.
    Registry reg;
    reg.set_current_owner("hostile");
    const HostApi api = reg.make_host_api();
    void* ctx = api.ctx;

    // Массив ровно по потолку: запредельный счётчик ниже передаётся как `size + 1`, и без
    // проверки верхнего конца цикл уходит за последний элемент — приманка для ASan.
    std::vector<std::string> owned(PLUGIN_MAX_SYSTEM_DEPS);
    std::vector<const char*> deps;
    for (int i = 0; i < PLUGIN_MAX_SYSTEM_DEPS; ++i) {
        owned[static_cast<std::size_t>(i)] = "dep" + std::to_string(i);
        deps.push_back(owned[static_cast<std::size_t>(i)].c_str());
    }

    // --- ECS-системы: строка, обработчик, оба конца диапазона счётчика, дыра в массиве ---
    api.register_ecs_system(ctx, nullptr, deps.data(), 1, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 0, "null id must not register an ecs-system");
    api.register_ecs_system(ctx, "s", deps.data(), 1, nullptr);
    check(reg.count(EXT_ECS_SYSTEM) == 0, "null fn must not register an ecs-system");
    api.register_ecs_system(ctx, "s", nullptr, 1, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 0, "null after[] with n_after>0 must not register");
    api.register_ecs_system(ctx, "s", deps.data(), -1, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 0, "negative n_after must not register");
    api.register_ecs_system(ctx, "s", deps.data(), PLUGIN_MAX_SYSTEM_DEPS + 1, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 0, "n_after above the ceiling must not register");

    // Дыра посреди массива: отказ обязан быть ЦЕЛИКОМ. Система, у которой хост тихо выбросил
    // часть зависимостей, встанет в расписании не туда — и разойдётся с задумкой плагина молча.
    const char* holed[3] = {"a", nullptr, "b"};
    api.register_ecs_system(ctx, "s", holed, 3, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 0, "a null element inside after[] must refuse the whole call");

    // Позитивный контроль на самой границе: потолок — включительно, иначе этот ассерт краснеет.
    api.register_ecs_system(ctx, "at-ceiling", deps.data(), PLUGIN_MAX_SYSTEM_DEPS, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 1, "exactly PLUGIN_MAX_SYSTEM_DEPS deps must register");
    api.register_ecs_system(ctx, "plain", deps.data(), 0, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 2, "a well-formed ecs-system must register");
    // Зависимости доехали целиком, а не обрезанным префиксом: счётчик выше этого не различает.
    // Поиск по ИМЕНИ, а не по индексу: перестань любой отказ выше быть отказом — и индекс 0 молча
    // станет другой записью, а оба ассерта останутся зелёными, проверяя не тот объект.
    const EcsSystem* ceiling = nullptr;
    for (const auto& sys : reg.ecs_systems()) {
        if (sys.id == "at-ceiling") ceiling = &sys;
    }
    check(ceiling != nullptr, "the at-ceiling system must be the one that registered");
    if (ceiling != nullptr) {
        check(ceiling->after.size() == static_cast<std::size_t>(PLUGIN_MAX_SYSTEM_DEPS),
              "all declared deps must arrive, not a prefix");
        check(ceiling->after.back() == owned.back(), "the last dep must arrive intact");
    }

    // Нет зависимостей — массива может не быть вовсе, и это КОНТРАКТ, а не поблажка: сторож
    // `after` без условия `n > 0` отказывал бы добросовестному плагину, и ни один ассерт выше
    // этого не увидел бы — все они подают `deps.data()`.
    api.register_ecs_system(ctx, "nodeps", nullptr, 0, sys_noop);
    check(reg.count(EXT_ECS_SYSTEM) == 3, "no deps at all means after[] may be null");

    // --- Кодеки: строка, обработчик, дубликат (у обоих соседей отказ был, у кодека не было) ---
    api.register_asset_codec(ctx, nullptr, decode_noop);
    check(reg.count(EXT_ASSET_CODEC) == 0, "null fourcc must not register a codec");
    api.register_asset_codec(ctx, "RLE0", nullptr);
    check(reg.count(EXT_ASSET_CODEC) == 0, "null fn must not register a codec");
    api.register_asset_codec(ctx, "RLE0", decode_noop);
    check(reg.count(EXT_ASSET_CODEC) == 1, "a well-formed codec must register");
    api.register_asset_codec(ctx, "RLE0", decode_noop);
    check(reg.count(EXT_ASSET_CODEC) == 1, "a duplicate fourcc must be rejected");

    // --- Именованные слоты: id, обработчик и заголовок панели приходят из того же кадра ---
    api.register_render_pass(ctx, nullptr, reinterpret_cast<OpaqueFn>(&sys_noop));
    check(reg.count(EXT_RENDER_PASS) == 0, "null id must not register a render pass");
    // Позитивный контроль на КАЖДЫЙ слот, а не только на два из четырёх: без него «сторож
    // сработал» неотличимо от «вход мёртв», и мёртвый танк проходил бы гейт зелёным.
    api.register_render_pass(ctx, "pass", reinterpret_cast<OpaqueFn>(&sys_noop));
    check(reg.count(EXT_RENDER_PASS) == 1, "a well-formed render pass must register");
    api.register_input_source(ctx, "src", nullptr);
    check(reg.count(EXT_INPUT_SOURCE) == 0, "null fn must not register an input source");
    api.register_input_source(ctx, "src", reinterpret_cast<OpaqueFn>(&sys_noop));
    check(reg.count(EXT_INPUT_SOURCE) == 1, "a well-formed input source must register");
    api.register_audio_bus(ctx, "bus", reinterpret_cast<OpaqueFn>(&sys_noop));
    check(reg.count(EXT_AUDIO_BUS) == 1, "a well-formed audio bus must register");
    api.register_ui_panel(ctx, "panel", nullptr, draw_noop);
    check(reg.count(EXT_UI_PANEL) == 0, "null title must not register a ui panel");
    api.register_ui_panel(ctx, "panel", "Panel", draw_noop);
    check(reg.count(EXT_UI_PANEL) == 1, "a well-formed ui panel must register");

    // --- Бэкенд достижений: указатель на структуру и обязательное поле внутри неё ---
    api.register_achievement_backend(ctx, "b", nullptr);
    check(reg.count(EXT_ACHIEVEMENT_BACKEND) == 0, "null backend must not register");
    static AchBackendApi no_unlock{};
    api.register_achievement_backend(ctx, "b", &no_unlock);
    check(reg.count(EXT_ACHIEVEMENT_BACKEND) == 0, "a backend without unlock must not register");
    static AchBackendApi good{};
    good.unlock = unlock_noop;
    api.register_achievement_backend(ctx, "b", &good);
    check(reg.count(EXT_ACHIEVEMENT_BACKEND) == 1, "a well-formed backend must register");

    // --- Потолок длины: строка без терминатора в пределах потолка ---
    // Буфер РОВНО по потолку и без нуля: `std::string(p)` ушёл бы за его конец до первого
    // случайного нуля. Приманка для ASan ровно та же, что у `after[]` выше, только по строке.
    std::vector<char> unterminated(PLUGIN_MAX_ID, 'x');
    api.register_render_pass(ctx, unterminated.data(), reinterpret_cast<OpaqueFn>(&sys_noop));
    check(reg.count(EXT_RENDER_PASS) == 1, "an unterminated id must not register");

    // --- Контекст хоста: ПЕРВЫЙ аргумент всех восьми входов, и он тоже из чужого кадра ---
    // Без сторожа это вызов метода по нулевому указателю — UB, которое барьер `guarded` поймать
    // не может в принципе: это не исключение. Проверяется весь ряд, а не один вход: сторож,
    // забытый в одном танке, был бы неотличим от поставленного во всех.
    const std::size_t ecs_before = reg.count(EXT_ECS_SYSTEM);
    api.register_ecs_system(nullptr, "s", deps.data(), 0, sys_noop);
    api.register_asset_codec(nullptr, "NUL0", decode_noop);
    api.register_render_pass(nullptr, "p", reinterpret_cast<OpaqueFn>(&sys_noop));
    api.register_input_source(nullptr, "i", reinterpret_cast<OpaqueFn>(&sys_noop));
    api.register_audio_bus(nullptr, "a", reinterpret_cast<OpaqueFn>(&sys_noop));
    api.register_ui_panel(nullptr, "u", "U", draw_noop);
    api.register_achievement_backend(nullptr, "n", &good);
    api.log(nullptr, "null ctx must not be a crash");
    check(reg.count(EXT_ECS_SYSTEM) == ecs_before && reg.count(EXT_ASSET_CODEC) == 1 &&
              reg.count(EXT_RENDER_PASS) == 1 && reg.count(EXT_INPUT_SOURCE) == 1 &&
              reg.count(EXT_AUDIO_BUS) == 1 && reg.count(EXT_UI_PANEL) == 1 &&
              reg.count(EXT_ACHIEVEMENT_BACKEND) == 1,
          "a null host ctx must not register anywhere and must not crash the host");

    // Лог без строки. Утверждения тут НЕТ намеренно, и молчанием это быть не должно: libc обоих
    // раннеров печатает за `%s` от nullptr то же `(null)`, что печатаем мы, — снятый сторож этот
    // вызов не красит НИ НА ОДНОЙ из трёх ОС. Вызов остаётся как проход по восьмому входу целиком
    // (хост обязан пережить его, а не упасть); почему сторож всё-таки стоит — в host_api.cpp.
    api.log(ctx, nullptr);
    api.log(ctx, "abi gate alive");

    // Отказы обязаны быть отказами РЕГИСТРАЦИИ, а не порчи реестра: расписание после всего этого
    // обязано строиться, иначе выше мы намеряли пустоту вместо целости.
    bool ok = false;
    const std::vector<ScheduledSystem> plan = reg.schedule(&ok);
    check(ok && plan.size() == 3, "the registry must still schedule after the hostile calls");

    if (failures != 0) {
        std::fprintf(stderr, "[plugin-abi] FAIL: %d check(s)\n", failures);
        return 1;
    }
    std::printf("[plugin-abi] PASS deps_ceiling=%d ecs=%zu codecs=%zu backends=%zu\n",
                PLUGIN_MAX_SYSTEM_DEPS, reg.count(EXT_ECS_SYSTEM), reg.count(EXT_ASSET_CODEC),
                reg.count(EXT_ACHIEVEMENT_BACKEND));
    return 0;
}
