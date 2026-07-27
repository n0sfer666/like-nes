#include "platform_env.hpp"

#include <cstdio>
#include <string>

// Шов окружения — единственное место, где две реализации могут разойтись молча: обе отдают
// строку, и «строка приехала битой» выглядит как «ассета нет». Тест сам выставляет ручки, а не
// принимает их из шага CI: в шелле пришлось бы цитировать кириллицу и 900-символьное значение
// по-разному на трёх ОС, и проверял бы он шелл.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "[platform-env] FAIL: %s\n", what);
        ++fails;
    }
}

const char* KEY = "LIKENES_ENV_SEAM_TEST";

} // namespace

int main() {
    check(platform::env_put(KEY, nullptr), "removing an unset variable must succeed");
    std::string out = "untouched";
    check(!platform::env_var(KEY, out), "unset variable reads as absent");
    check(out == "untouched", "absent variable leaves out alone");
    check(!platform::env_has(KEY), "env_has on an unset variable");

    // Кириллица — то, ради чего шов и заведён: узкий getenv отдаёт значение в ANSI-кодовой
    // странице, и путь профиля `C:\Users\Пётр\` приезжает с подменёнными символами.
    const std::string unicode = "D:/юникод тест-каталог/game.bundle";
    check(platform::env_put(KEY, unicode.c_str()), "setting a unicode value");
    check(platform::env_var(KEY, out) && out == unicode, "unicode value survives the round-trip");
    check(platform::env_has(KEY), "env_has on a set variable");

    // Длиннее стартового буфера (256 wchar): в этой ветке GetEnvironmentVariableW возвращает
    // ТРЕБУЕМЫЙ размер вместе с нулём, а не длину значения. Перепутать одно с другим — потерять
    // последний символ, и заметно это будет только на длинных путях.
    const std::string long_value(900, 'x');
    check(platform::env_put(KEY, long_value.c_str()), "setting a value past the initial buffer");
    check(platform::env_var(KEY, out) && out == long_value, "long value comes back whole");

    // Отказ записи не имеет права стереть прежнее значение. Формулировка портабельна намеренно:
    // POSIX-setenv берёт произвольные байты и запись проходит, а на Windows `\xC3(` — оборванная
    // UTF-8-последовательность, widen её отвергает, и до фикса пустая wstring уходила в
    // SetEnvironmentVariableW как команда УДАЛИТЬ, после чего шов рапортовал успех.
    const char* BROKEN_UTF8 = "\xC3\x28";
    check(platform::env_put(KEY, "kept"), "setting the value guarded below");
    std::string before;
    check(platform::env_var(KEY, before) && before == "kept", "guarded value is in place");
    const bool written = platform::env_put(KEY, BROKEN_UTF8);
    std::string after;
    const bool present = platform::env_var(KEY, after);
    // Утверждается «одно из двух», а не «запись падает»: сравнивать надо ИСХОД с ЭФФЕКТОМ, иначе
    // ассерт зелен и на до-фиксном коде, который рапортовал успех, стерев переменную.
    check(present && after == (written ? BROKEN_UTF8 : before),
          "a put either takes effect or leaves the old value alone");

    check(platform::env_put(KEY, nullptr), "removing a set variable");
    check(!platform::env_var(KEY, out), "removed variable reads as absent");

    // Пустое значение не проверяется намеренно: Windows это состояние не представляет (и cmd.exe,
    // и SetEnvironmentVariableW на пустой строке переменную удаляют), так что ассерт сверял бы
    // поведение ОС, а не шва. Контракт в platform_env.hpp запрещает на это различие опираться.

    if (fails == 0) {
        std::printf("platform-env: PASS\n");
        return 0;
    }
    std::printf("platform-env: FAIL (%d)\n", fails);
    return 1;
}
