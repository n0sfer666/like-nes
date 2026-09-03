#!/usr/bin/env bash
# `library.bundle` (спека #18) сверяется СТРОЖЕ, чем `game.bundle`: обе его секции пекутся чистыми
# парсерами — таблица материалов и текст модуля, — ни tint, ни basisu им не нужно, поэтому он
# перепекается целиком и сравнивается БАЙТ В БАЙТ. Без этого правка `library.mat` без перепекания
# доезжает молча, а на бандле стоит живой прогон образца (гейт 9 спеки).
#
# Отдельным файлом по той же границе, что и `check_debug_golden.sh`: предмет у него свой —
# `library.bundle`, — и в логе обязано стоять имя упавшей проверки, а не «что-то из трёх
# разошлось». Зовётся напрямую (`bash scripts/check_library_bundle.sh`) и из этапа `bundle`.
set -uo pipefail

rc=0
tmp=$(mktemp -d) || exit 1
# Печём из КОПИИ, а не из дерева: у `assetc --materials` был трёхаргументный вид, где третий путь
# был выходом, и устаревший бинарь в `build-ci` дважды написал бандл ПОВЕРХ `sprite_effects.wgsl`.
# Гейт при этом честно сообщал о расхождении — им же и вызванном. Копия отбирает у него право
# портить то, что он проверяет.
cp engine/material/library/library.mat engine/material/library/sprite_effects.wgsl \
    engine/light/library/lights.txt "$tmp/" || {
    rm -rf "$tmp"; exit 1
}
if ! build-ci/assetc --materials "$tmp/library.mat" "$tmp/sprite_effects.wgsl" \
        "$tmp/library.bundle" --lights "$tmp/lights.txt" >"$tmp/bake.txt"; then
    echo "  library.bundle не перепёкся — assetc --materials упал"
    cat "$tmp/bake.txt"
    rm -rf "$tmp"
    exit 1
fi
if cmp -s "$tmp/library.bundle" example_ugly_game/assets/library.bundle; then
    echo "  library.bundle совпал с перепечённым"
else
    echo "  library.bundle ОТСТАЛ от engine/material/library или engine/light/library"
    rc=1
fi
# Второй контроль — на КОНЦЫ СТРОК: вход с LF и вход с CRLF обязаны дать одни и те же байты. Иначе
# артефакт зависит от настройки клона, а не от содержимого: `core.autocrlf=true` на Windows-раннере
# дал 6192 байта против 6080 и покрасил гейт, не сказав ни слова про материалы (прогон 192ed67).
# Оба варианта строятся ИЗ ОДНОГО входа, а не сверяются с тем, что дал checkout: у `lights.txt` нет
# записи в `.gitattributes`, на Windows он приезжает уже с CRLF, и прежняя форма («подстановка
# обязана изменить файл») краснела ровно там, где вход и так нёс проверяемые концы строк
# (прогон 4863a46). Утверждение о ФАЙЛЕ — два его варианта различны — верно на любой ОС.
# `library.mat` берётся вместе с модулем: сейчас таблица CRLF-независима лишь потому, что
# `text::trim` держит `\r` в наборе, и первая же правка парсера мимо `trim` вернула бы ту же
# зависимость по второму входу.
if ! command -v python3 >/dev/null 2>&1; then
    echo "  контроль концов строк ПРОПУЩЕН: нет python3 — пропуск не вердикт"
    rc=1
else
    eol_ok=1
    for f in library.mat sprite_effects.wgsl lights.txt; do
        case "$f" in lights.txt) sdir=engine/light/library ;; *) sdir=engine/material/library ;; esac
        if ! python3 -c "import sys; d=open(sys.argv[1],'rb').read().replace(b'\r\n', b'\n'); open(sys.argv[2],'wb').write(d); open(sys.argv[3],'wb').write(d.replace(b'\n', b'\r\n'))" \
                "$sdir/$f" "$tmp/lf-$f" "$tmp/crlf-$f"; then
            echo "  контроль концов строк: подстановка в $f упала"; rc=1; eol_ok=0
        elif cmp -s "$tmp/lf-$f" "$tmp/crlf-$f"; then
            # Файл, у которого оба варианта совпали, доказывает не независимость от концов строк, а
            # собственную непригодность для контроля: сравнивать в нём нечего.
            echo "  контроль концов строк: варианты $f совпали — сравнивать нечего"; rc=1; eol_ok=0
        fi
    done
    if [ $eol_ok -eq 1 ]; then
        for v in lf crlf; do
            if ! build-ci/assetc --materials "$tmp/$v-library.mat" "$tmp/$v-sprite_effects.wgsl" \
                    "$tmp/$v.bundle" --lights "$tmp/$v-lights.txt" >/dev/null; then
                echo "  library.bundle: перепекание из копии с $v упало"; rc=1
            elif ! cmp -s "$tmp/$v.bundle" example_ugly_game/assets/library.bundle; then
                echo "  library.bundle зависит от концов строк ($v) — байты не функция содержимого"
                rc=1
            fi
        done
    fi
fi
# Третье утверждение — ВАЛИДАЦИЯ (гейт 2 спеки #18): тот же прогон бейка собрал библиотеку на
# бэкендах этой машины. Число ассертится ЧИСЛОМ, потому что код возврата у бейка нулевой и там, где
# не поднялся ни один адаптер: «шейдер валиден» и «проверять было негде» различает только эта
# строка. Имена бэкендов не ассертятся локально — их набор у машины владельца свой, а
# «на macOS обязан быть Metal» стоит в шаге CI, где окружение известно.
checked=$(sed -n 's/.*materials: \([0-9][0-9]*\) of [0-9][0-9]* target backend(s) checked/\1/p' \
    "$tmp/bake.txt")
if [ -z "$checked" ]; then
    echo "  бейк не сказал, сколько бэкендов проверил — валидация молчит"
    rc=1
elif [ "$checked" -lt 1 ]; then
    echo "  ни один бэкенд не проверил библиотеку — валидации на этой машине не было"
    rc=1
else
    grep '\[assetc\] materials:' "$tmp/bake.txt" | sed 's/^/  /'
fi
# Позитивный контроль ВАЛИДАЦИИ: заведомо битый WGSL обязан быть отбит ненулевым кодом, названной
# позицией и НЕ оставленным бандлом. Без него утверждение выше держится на том, что библиотека
# валидна, и осталось бы зелёным при выключенной проверке.
if [ -n "${checked:-}" ] && [ "${checked:-0}" -ge 1 ]; then
    python3 -c "import sys; l=open(sys.argv[1]).read().split(chr(10)); l.insert(9, 'let broken_here: f32 = ;'); open(sys.argv[2],'w').write(chr(10).join(l))" \
        engine/material/library/sprite_effects.wgsl "$tmp/bad.wgsl"
    if build-ci/assetc --materials "$tmp/library.mat" "$tmp/bad.wgsl" "$tmp/bad.bundle" \
            --lights "$tmp/lights.txt" >"$tmp/bad.txt" 2>&1; then
        echo "  БИТЫЙ ШЕЙДЕР ПРИНЯТ — валидация не отбивает несобираемый WGSL"
        rc=1
    elif ! grep -q "bad.wgsl:[0-9][0-9]*:[0-9][0-9]*: error:" "$tmp/bad.txt"; then
        echo "  отказ без диагностики file:line:col — панель редактора её не разберёт"
        cat "$tmp/bad.txt"
        rc=1
    elif [ -f "$tmp/bad.bundle" ]; then
        echo "  отвергнутая библиотека оставила бандл на диске"
        rc=1
    fi
fi
# Позитивный контроль ТЕМ ЖЕ сравнением: испорченная копия обязана быть отбита. Без него первая
# же опечатка в пути делает гейт вечно зелёным — тот же класс, что ловит правило `vacuous-gate`.
printf 'x' >> "$tmp/library.bundle"
if cmp -s "$tmp/library.bundle" example_ugly_game/assets/library.bundle; then
    echo "  сравнение не отбивает испорченную копию — гейт зелен вакуумно"
    rc=1
fi
rm -rf "$tmp"
exit $rc
