# Замер кадров #26 В7

`python3 scripts/frame_stats_verdict.py --selftest` — 123 контроля схемы и настоящего CLI.
`python3 scripts/frame_stats_verdict.py report.log` проверяет один schema1 JSON-отчёт либо строку
`[frame-stats] {…}` среди системных логов. `--budget report.log` дополнительно требует строго
`100 * misses < intervals + misses`. Сам скрипт не подтверждает происхождение замера с телефона.

Обязательны frames/intervals/misses, отдельные work/interval p50/p99 в ns, underruns,
save_ms, save_ok и complete. Дубликаты ключей, missing fields, NaN/Infinity, bool вместо integer,
отрицательные/несогласованные числа и несколько отчётов — отказ. Пустые выборки и неполный
capture не проходят. Измеренный save_ms=0 допустим только при успешной записи. Порогов
work-p99 и underruns сверх требований нет: значения выводятся, а не скрываются.

Generic collector в framework_core принимает ns, живёт вне snapshot и не читает часы.
Copy/move запрещены: иначе копия vector теряет reserve, а moved-from сохраняет ложный init.
Capacity выделяется до кадра, record не аллоцирует; summary сортирует вне замера. Переполнение
делает результат incomplete. Интервал0 исключает первый/возобновлённый cadence sample.
Nearest60Hz half-up, quotient/remainder без умножения u64 на60; misses=max(0,periods−1).
Процентили — upper indices N/2 и floor99N/100 как в прежнем resim bench.

Первый пакет проверяет арифметику, empty/capacity/reset, независимые границы, noheap controls,
actual source mutants, SDK header/link, R/D/SAN и CLI. Второй пакет подключает реальный rendering,
меню#25, Touch, MusicStream/device и измеренную SDK-запись. CPU/null/emulator прогоны не закрывают
телефонный критерий <1% и физические ввод/звук; они остаются в owner-verification/runbook.
