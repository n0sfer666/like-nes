# Строки и видимый текст (#26 В5)

Покрытие глифов проверяет настоящий strings baker по named FontSrc, включая алфавиты grid.*.
`framework_strings_test` и `framework_strings_limits_test`: missing glyph с key/U+XXXX,
fallback count, strict refusal, дубликаты/коллизии, caps и malformed wire. Assetc вызывает тот
же baker, like_nes_bake автоматически strict в релизных конфигурациях. CI — Release/Debug/SAN,
fuzz strings — index20. Улики и формат: `.context/notes/26-b5.md`.

`python3 scripts/check_ui_text.py --selftest` — локальный lint и отдельный шаг CI на трёх ОС.
В #26 судятся фикстуры, реальные screen sources подключаются с #29. Команда для потребителя:
`python3 scripts/check_ui_text.py --strings path/strings.tsv --screens path/screen.cpp path/pause.cpp`.
Пустой список/нет таблицы/битый TSV/неизвестный source/missing file — отказ, а не зелёный обход.
Каждый реальный scan также проверяет положительный контроль детектора литералов.

Политика намеренно консервативна: все строковые и печатные символьные литералы в указанных
экранных units запрещены, кроме include, прямых ключей известных UI API и диагностик.
`button/slider/field/glyph_grid` принимают ключ вторым аргументом, `find` — первым;
ключ обязан буквально присутствовать в TSV. `ui_id` принимает ASCII machine key вторым,
`UiListItem{"machine-id","label.key"}` — id/key, `UiGridLabels` — два ключа. Определения
произвольных alias с литералом и bare массивы агрегатов не получают исключение: используйте
явные UiListItem на каждой строке. Статические подписи задавайте ключом прямо в UI call.
Разрешены пустой buffer seed `""` и ровно char escapes `\0`, `\n`, `\r`, `\t`. Начальное видимое содержимое
поля берётся из Strings, динамические данные игрока остаются buffer.

Диагностики printf/fprintf/assert/static_assert принимают только непосредственные литералы
аргумента; вложенный draw_text("literal") не получает их разрешение. Имена методов и чужих namespaces
не считаются std diagnostics. Локальный #define имени из реестра отключает его исключение
во всём unit; стандартный digraph `%:` распознаётся как `#`. Backslash-LF/CRLF
полностью запрещены в screen units (включая raw literals и комментарии): приближённое
выполнение phase2 нарушало raw-string restoration и могло скрыть следующий UI literal. Unicode identifier не делится на ASCII suffix. Комментарий/маркер
внутри литерала не отключает правило. Конкатенация ключа, raw/escaped ключ и неизвестный
ключ отказываются; raw/UTF-prefixed видимый текст тоже отказывается. Отдельных suppressions нет.

Это синтаксический guard экранных units, не доказательство потока данных произвольного C++:
сборку текста из чисел, текст в других units, function/type shadowing известных имён
и произвольные пользовательские API судит review.
Реестр API намеренно закрытый; новый способ статической подписи добавляется с bad/good fixtures.

Selftest: 79 controls, в том числе настоящий CLI с Unicode/пробелами в пути, положительные
и отрицательные экраны, UTF/raw/adjacent/nested literals, номера строк, TSV/empty-scan refusal.
TSV читает не более16MiB+1, отказывается от NUL/более4096 ключей, допускает пустые строки
как runtime parser; C++ BOM отказывается явной диагностикой по правилам проекта.
Изменение детектора проверяется намеренными поломками production Python modules, не только
копией ожидаемого результата.
