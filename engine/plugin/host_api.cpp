// Граница C-ABI отделена от самого реестра не по счётчику строк, а по предмету: здесь КАДР ЧУЖОГО
// КОДА — указатели и счётчики, которым нельзя верить, и обязательство ничего не выпустить наружу
// в виде исключения. В `registry.cpp` рядом — контейнер расширений, которому всё это уже пришло
// проверенным. Пока обе ответственности жили одним файлом, сторожа читались как часть учёта.
#include "registry.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

static Registry* self(void* ctx) { return static_cast<Registry*>(ctx); }

namespace {

// Барьер границы C-ABI. Танк зовут из кадра плагина, собранного ЧУЖИМ компилятором, и исключение,
// пролетевшее сквозь такой кадр, — неопределённое поведение, а не отказ регистрации. `noexcept`
// превращает пролёт в предсказуемый `std::terminate`, а перехват здесь — в отказ ОДНОЙ записи:
// хост доживает до конца регистрации и говорит в лог, какой именно вход отказал.
template <typename Body>
void guarded(const char* what, Body body) noexcept {
    try {
        body();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[host] %s rejected: %s\n", what, e.what());
    } catch (...) {
        std::fprintf(stderr, "[host] %s rejected: unknown exception\n", what);
    }
}

// Любой указатель из кадра плагина: `std::string` от nullptr — UB раньше любой нашей проверки, а
// пустой обработчик доживёт до тика или до декодирования и упадёт там, где по логу виноват
// движок, а не тот, кто его зарегистрировал. Поэтому сторож стоит на ВХОДЕ, до использования.
template <typename Ptr>
bool null_arg(Ptr p, const char* what, const char* field) {
    if (p != nullptr) return false;
    std::fprintf(stderr, "[host] %s rejected: %s is null\n", what, field);
    return true;
}

// Строка из кадра плагина не обязана быть NUL-терминированной: `std::string(p)` читал бы чужую
// память до первого случайного нуля — тот же класс, что аудит уже закрыл потолком имён у пресетов
// ввода (`MAX_NAME` в preset_format.hpp). Терминатор ищется В ПРЕДЕЛАХ потолка: дальше читать
// нечем, а в логе стоит имя поля, а не адрес хоста в чужом стеке. Оговорка без прикрас: по совсем
// битому указателю это ОГРАНИЧИВАЕТ ущерб, а не проверяет его — валидировать чужой указатель
// нечем в принципе, и делать вид, что можно, было бы хуже, чем не делать ничего.
bool take_str(const char* p, const char* what, const char* field, std::string* out) {
    if (null_arg(p, what, field)) return false;
    const void* end = std::memchr(p, '\0', PLUGIN_MAX_ID);
    if (end == nullptr) {
        std::fprintf(stderr, "[host] %s rejected: %s is not terminated within %d bytes\n", what, field,
                     PLUGIN_MAX_ID);
        return false;
    }
    out->assign(p, static_cast<std::size_t>(static_cast<const char*>(end) - p));
    return true;
}

// `extra_field` — имя поля из ОБЪЯВЛЕНИЯ `HostApi`, а не имя параметра этой функции: отказ читает
// автор плагина, и «extra is null» заставило бы его гадать, какой из аргументов его же вызова
// имеется в виду. Ровно та болезнь, от которой заводилась вся граница: в логе имя хоста вместо
// имени того, кто ошибся. У трёх слотов из четырёх своего поля нет — им сюда приезжает константа.
void thunk_named(void* ctx, ExtKind kind, const char* what, const char* id, const char* extra,
                 const char* extra_field, void* fn) {
    guarded(what, [&] {
        if (null_arg(ctx, what, "ctx") || null_arg(fn, what, "fn")) return;
        std::string sid;
        std::string sextra;
        if (!take_str(id, what, "id", &sid) || !take_str(extra, what, extra_field, &sextra))
            return;
        self(ctx)->add_named(kind, sid, sextra, fn);
    });
}

}  // namespace

static void thunk_ecs(void* ctx, const char* id, const char* const* after, int32_t n, SimSystemFn fn) noexcept {
    guarded("ecs-system", [&] {
        if (null_arg(ctx, "ecs-system", "ctx") || null_arg(fn, "ecs-system", "fn")) return;
        std::string sid;
        if (!take_str(id, "ecs-system", "id", &sid)) return;
        // Счётчик знаковый, и КАЖДЫЙ конец диапазона отбивается по своей причине, а не для
        // симметрии. Отрицательный даёт цикл ниже в НОЛЬ итераций: система встала бы в реестр с
        // пустым списком зависимостей, хотя плагин их объявил, — и разошлась бы с его задумкой
        // молча, уже в расписании. Запредельный уводит чтение за конец `after[]`, в чужую память.
        if (n < 0 || n > PLUGIN_MAX_SYSTEM_DEPS) {
            std::fprintf(stderr, "[host] ecs-system '%s' rejected: n_after=%d outside [0, %d]\n",
                         sid.c_str(), n, PLUGIN_MAX_SYSTEM_DEPS);
            return;
        }
        // Условие `n > 0` — не осторожность, а КОНТРАКТ: система без зависимостей вправе не
        // приносить массив вовсе, и отказ ей был бы отказом добросовестному плагину.
        if (n > 0 && null_arg(after, "ecs-system", "after")) return;
        // Без `reserve`, и это не забывчивость: на отрицательном `n` он бросает `length_error`,
        // барьер ниже молча ловит его — и сторож `n < 0` выше становится непроверяемым, потому
        // что отказ приходит по чужой причине. Шестьдесят четыре строки того не стоят.
        std::vector<std::string> deps;
        for (int32_t i = 0; i < n; ++i) {
            // Отказ ЦЕЛИКОМ, а не пропуск элемента: система, у которой хост тихо выбросил часть
            // зависимостей, встанет в расписании не туда — и разойдётся с задумкой плагина молча.
            std::string dep;
            if (!take_str(after[i], "ecs-system", "after[i]", &dep)) return;
            deps.push_back(std::move(dep));
        }
        self(ctx)->add_ecs_system(sid, std::move(deps), fn);
    });
}
static void thunk_codec(void* ctx, const char* fourcc, AssetDecodeFn fn) noexcept {
    guarded("asset-codec", [&] {
        if (null_arg(ctx, "asset-codec", "ctx") || null_arg(fn, "asset-codec", "fn")) return;
        std::string sfourcc;
        if (!take_str(fourcc, "asset-codec", "fourcc", &sfourcc)) return;
        self(ctx)->add_asset_codec(sfourcc, fn);
    });
}
static void thunk_render(void* ctx, const char* id, OpaqueFn fn) noexcept {
    thunk_named(ctx, EXT_RENDER_PASS, "render-pass", id, "", "", reinterpret_cast<void*>(fn));
}
static void thunk_input(void* ctx, const char* id, OpaqueFn fn) noexcept {
    thunk_named(ctx, EXT_INPUT_SOURCE, "input-source", id, "", "", reinterpret_cast<void*>(fn));
}
static void thunk_audio(void* ctx, const char* id, OpaqueFn fn) noexcept {
    thunk_named(ctx, EXT_AUDIO_BUS, "audio-bus", id, "", "", reinterpret_cast<void*>(fn));
}
static void thunk_ui(void* ctx, const char* id, const char* title, UiDrawFn fn) noexcept {
    thunk_named(ctx, EXT_UI_PANEL, "ui-panel", id, title, "title", reinterpret_cast<void*>(fn));
}
static void thunk_ach_backend(void* ctx, const char* id, const AchBackendApi* backend) noexcept {
    guarded("achievement backend", [&] {
        if (null_arg(ctx, "achievement backend", "ctx")) return;
        std::string sid;
        if (!take_str(id, "achievement backend", "id", &sid)) return;
        if (null_arg(backend, "achievement backend", "backend")) return;
        if (null_arg(backend->unlock, "achievement backend", "unlock")) return;
        self(ctx)->add_backend(sid, backend);
    });
}
// Единственный сторож этого файла, которого НЕ КРАСИТ ни одна мутация, и это записано, а не
// упущено: `%s` от nullptr — UB по стандарту, но обе libc наших раннеров печатают на его месте
// `(null)`, то есть ровно то же, что печатаем мы. Снять его — значит оставить в коде UB ради
// строки; проверить его — значит перехватывать stderr, то есть заводить файловый ввод-вывод в
// тесте мимо шва. Остаётся как обязательство перед стандартом, а не как поведение.
static void thunk_log(void*, const char* msg) noexcept {
    std::fprintf(stderr, "[plugin] %s\n", msg == nullptr ? "(null)" : msg);
}

// Гейт на сам барьер: чёрный ящик не умеет заставить чужой кадр бросить, зато снятое с любого из
// восьми входов `noexcept` краснеет ЗДЕСЬ, на сборке, — а без него перехват выше бессмыслен.
static_assert(noexcept(thunk_ecs(nullptr, nullptr, nullptr, 0, nullptr)) &&
                  noexcept(thunk_codec(nullptr, nullptr, nullptr)) &&
                  noexcept(thunk_render(nullptr, nullptr, nullptr)) &&
                  noexcept(thunk_input(nullptr, nullptr, nullptr)) &&
                  noexcept(thunk_audio(nullptr, nullptr, nullptr)) &&
                  noexcept(thunk_ui(nullptr, nullptr, nullptr, nullptr)) &&
                  noexcept(thunk_ach_backend(nullptr, nullptr, nullptr)) &&
                  noexcept(thunk_log(nullptr, nullptr)),
              "every plugin-facing thunk must be noexcept: an exception crossing a C-ABI frame is UB");

HostApi Registry::make_host_api() {
    HostApi api{};
    api.ctx = this;
    api.api_version = PLUGIN_API_VERSION;
    api.register_ecs_system = thunk_ecs;
    api.register_asset_codec = thunk_codec;
    api.register_render_pass = thunk_render;
    api.register_input_source = thunk_input;
    api.register_audio_bus = thunk_audio;
    api.register_ui_panel = thunk_ui;
    api.register_achievement_backend = thunk_ach_backend;
    api.log = thunk_log;
    return api;
}
