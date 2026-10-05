# В8б — шрифт monogram и титры (рабочие заметки, спека #24)

Решения владельца (опрос 2026-10-05): источник — `monogram-bitmap.json` (PNG автора только ASCII,
96×96), атлас печёт бейк; титры — F1 поверх уровня; длинные строки — перенос по ширине; проверка T4
без GPU-голдена (пиксели на экране — владелец, сценарий В8в).

## Дизайн

- Манифест: `font|<имя>|bitmask|<путь>.json`, `credits|<имя>|<путь>` (3 поля). Имя `fonts`
  резервируется вместе с `tilemap/visual/objects/clips`; таблица титров носит имя записи, коллизию
  отбивает единственность имён манифеста.
- `font`: JSON `{"<один codepoint>": [строки-маски], …}`, бит i = столбец i (LSB слева), строк
  поровну у всех (1..64), строка < 2^16, codepoint < 0x20 и 0x7F — отказ. Атлас RGBA8: клетка
  max_w × line_height, 32 клетки в ряд, белый где бит, (0,0,0,0) иначе; текстура guid = имя шрифта.
  advance = max(mono, w + 1), mono = 1 + max ширина ASCII-глифов.
- Секция `fonts` (`LNFN` v1): FontRow{name, line_height, pad, texture_guid, glyph_offset,
  glyph_count, page_w, page_h, pad} 32 байта; FontGlyph{codepoint, x, y, w, advance, pad} 12 байт,
  строго по возрастанию codepoint.
- Секция `<имя credits>` (`LNCR` v1, framework/core): CreditRow{pack, author, license, url, attribution} 20 байт.
  Исходник — `credits.txt` гейта лицензий: `credit | pack | author | license | url`.
- Раскладка `text_layout` (graphics) → `GlyphPlace`; `gpu/text_quads` → `render::Quad`. Неизвестный
  символ и битый UTF-8 → `?`, счётчик unknown.
- Игра: `rumble_credits.*`, F1, текстура шрифта = `texture_count + 2`.
- fuzz +2 цели (`fonts`, `credits`): 14→16, 13→15 — пять мест (три грепа ci.yml,
  `preflight_build_rules.sh`, `.context/gates/fuzz-readers.md`).

## Прогресс

- [x] utf8 core, credits core, font graphics, import, layout, quads
- [x] assetc: записи, пекари, тесты
- [x] fuzz, CI-списки, SDK-заголовки
- [x] игра + headless-сводка + sdk_game_lib.sh + owner-setup.txt + bundle.hash
- [x] monogram в assets + LICENSES.toml + credits.txt/NOTICES.txt
- [x] ревью, коммит, CI (`0cb520f`, CI зелёный на трёх ОС)
- [x] В8в: гейт врезок на `games/neon-rumble/` и CMake (`1834473`), пять страниц en/ru (`7ed85a6`),
  сценарий §20 и runbook; PR раунда `dev` → `main`
