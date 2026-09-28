#pragma once
#include <string>

// Путь для лога и баг-репорта: домашний каталог заменён на `~`, чтобы имя учётной записи ОС не
// уезжало вместе с логом (аудит #21 A·3·12). Совпадение — только целым компонентом: `/home/al` не
// трогает `/home/alice`. Домашний каталог, равный корню (`/`, `C:\`), не маскирует ничего: иначе
// `~` встал бы перед каждым путём и прятал бы уже не имя, а сам путь.
namespace platform {

// Правила сравнения — параметр, а не #ifdef: так обе семантики проверяются тестом на любой ОС.
// Windows: регистр ASCII не различается (`C:\Users\X` и `c:\users\x` — один каталог), `/` ≡ `\`.
enum class PathRules { Posix, Windows };

std::string redact_home(const std::string& path, const std::string& home, PathRules rules);

// То же против домашнего каталога этого процесса: `HOME` на POSIX, `USERPROFILE` на Windows.
std::string redact_home(const std::string& path);

} // namespace platform
