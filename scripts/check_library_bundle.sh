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
cp engine/material/library/library.mat engine/material/library/sprite_effects.wgsl "$tmp/" || {
    rm -rf "$tmp"; exit 1
}
if ! build-ci/assetc --materials "$tmp/library.mat" \
        "$tmp/sprite_effects.wgsl" "$tmp/library.bundle" >/dev/null; then
    echo "  library.bundle не перепёкся — assetc --materials упал"
    rm -rf "$tmp"
    exit 1
fi
if cmp -s "$tmp/library.bundle" example_ugly_game/assets/library.bundle; then
    echo "  library.bundle совпал с перепечённым"
else
    echo "  library.bundle ОТСТАЛ от engine/material/library — перепеки: assetc --materials"
    rc=1
fi
# Второй контроль — на КОНЦЫ СТРОК: те же исходники с CRLF обязаны дать те же байты. Иначе
# артефакт зависит от настройки клона, а не от содержимого: `core.autocrlf=true` на
# Windows-раннере дал 6192 байта против 6080 и покрасил гейт, не сказав ни слова про материалы
# (прогон 192ed67). Утверждение локальное — красное на любой из трёх ОС, а не только там.
if command -v python3 >/dev/null 2>&1; then
    python3 -c "import sys; d=open(sys.argv[1],'rb').read(); open(sys.argv[2],'wb').write(d.replace(b'\n', b'\r\n'))" \
        engine/material/library/sprite_effects.wgsl "$tmp/crlf.wgsl"
    if ! build-ci/assetc --materials "$tmp/library.mat" "$tmp/crlf.wgsl" \
            "$tmp/crlf.bundle" >/dev/null; then
        echo "  library.bundle: перепекание из копии с CRLF упало"; rc=1
    elif ! cmp -s "$tmp/crlf.bundle" example_ugly_game/assets/library.bundle; then
        echo "  library.bundle зависит от концов строк — байты артефакта не функция содержимого"
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
