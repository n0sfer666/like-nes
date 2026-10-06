# Хеши драки равны на трёх ОС — `scripts/check_crossplay.py`

```
python3 scripts/check_crossplay.py <каталог>   # джоб crossplay в CI, после трёх ОС матрицы
python3 scripts/check_crossplay.py --selftest  # 12 контролей, в checks.json и в джобе
```

Спека #25, В1. Литералы в `framework_brawl_crossplay_test.cpp` ловят изменение хеша сценария, но
не расхождение между ОС: перепиновка на одной машине зеленит её прогон, а две другие падают
каждая своим `FAIL` без указания, кто из трёх прав. Джоб сравнивает прогоны ОС между собой.

- **Кто пишет.** Шаг «Brawl — crossplay hashes…» в `build-and-determinism` на каждой ОС зовёт
  `framework_brawl_crossplay_test --crossplay crossplay-<runner.os>.txt` и выкладывает файл
  артефактом `crossplay-<runner.os>` (`if-no-files-found: error`). Формат — `имя 0x<16 hex>\n`,
  файл открыт в `"wb"`: CRLF на Windows был бы расхождением байтов, а не хешей.
- **Кто сверяет.** Джоб `crossplay` (`needs: build-and-determinism`, Linux): самопроверка, скачивание
  по `pattern: crossplay-*` с `merge-multiple`, гейт.
- **Что отказ.** Нет файла ОС; пустой файл; CR в файле; последняя строка без LF; строка не
  `имя 0x<16 hex>`; имя сценария повторяется; число строк ≠ `CROSSPLAY_SCENARIOS` из
  `framework_brawl_crossplay.hpp`; константы в заголовке нет; файлы ОС не равны побайтово.
  Расхождение печатается по сценарию: `band-x-ends: Linux=0x…, Windows=0x…, macOS=0x…`; одинаковые
  хеши в другом порядке — отдельная находка.
- **Предел.** Падение любой ОС матрицы оставляет джоб `skipped`, а не красным: расхождение там
  уже названо литералом теста.

Позитивный контроль 2026-10-06: три файла с одного бинаря → PASS, 6 сценариев; один хеш Windows
изменён на единицу → `band-x-ends: Linux=…b2, Windows=…b3, macOS=…b2`, FAIL; файл macOS удалён →
`macOS: нет файла`, FAIL.
