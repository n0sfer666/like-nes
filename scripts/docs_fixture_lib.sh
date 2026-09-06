# shellcheck shell=bash
# Фабрика фикстурных деревьев документации (спека #19, вертикаль 1). Одна на ОБА набора
# самопроверки — про пару языков и про содержимое, — по тому же основанию, что release_ci_fixture_lib.sh:
# собери каждый набор дерево своей копией правил, и «утверждение отбило порчу» проверялось бы на
# дереве иного устройства, чем то, на котором утверждение проходит.
#
# Штампы ставит НАСТОЯЩИЙ scripts/docs_stamp.sh, а не printf внутри фабрики: штамповщик и гейт
# обязаны понимать один формат, и своя копия формата здесь молча разрешила бы им разойтись.

# Минимальное дерево, на котором проходят все шесть утверждений: два README с лицензионной частью,
# два индекса и два документа установки — то есть по одному критичному и одному некритичному
# переводу, потому что различие между ошибкой и предупреждением проверять больше не на чем.
# Ссылка с ЯКОРЕМ стоит в обеих ветках, и в ru-паре якорь кириллический: хостинг её не
# транслитерирует, а slug, написанный под латиницу, разошёлся бы ровно с той половиной дерева,
# которую читает русскоязычный.
docs_fixture_tree() {
  local d="$1" f
  mkdir -p "$d/docs/en/getting-started" "$d/docs/ru/getting-started"

  for f in LICENSE-MIT LICENSE-APACHE THIRD-PARTY.md; do
    printf 'fixture text of %s\n' "$f" > "$d/$f"
  done

  cat > "$d/README.md" <<'MD'
English · [Русский](README.ru.md)

# fixture

[Documentation](docs/en/index.md).

## License

`MIT OR Apache-2.0` — see [MIT](LICENSE-MIT), [Apache](LICENSE-APACHE)
and [third-party notices](THIRD-PARTY.md).
MD

  cat > "$d/README.ru.md" <<'MD'
[English](README.md) · Русский

# фикстура

[Документация](docs/ru/index.md).

## Лицензия

`MIT OR Apache-2.0` — тексты: [MIT](LICENSE-MIT), [Apache](LICENSE-APACHE)
и [уведомления третьих сторон](THIRD-PARTY.md).
MD

  cat > "$d/docs/en/index.md" <<'MD'
[English](index.md) · [Русский](../ru/index.md)

# docs

[Build](getting-started/build.md).
MD

  cat > "$d/docs/ru/index.md" <<'MD'
[English](../en/index.md) · [Русский](index.md)

# документация

[Сборка](getting-started/build.md).
MD

  cat > "$d/docs/en/getting-started/build.md" <<'MD'
[English](build.md) · [Русский](../../ru/getting-started/build.md)

# build

Back to the [index](../index.md) and to its [first section](../index.md#docs).
MD

  cat > "$d/docs/ru/getting-started/build.md" <<'MD'
[English](../../en/getting-started/build.md) · [Русский](build.md)

# сборка

Назад к [оглавлению](../index.md) и к его [первому разделу](../index.md#документация).
MD

  docs_fixture_stamp "$d" README.ru.md docs/ru/index.md docs/ru/getting-started/build.md
}

# Штампует названные переводы настоящим штамповщиком. Отдельной функцией, потому что зовут её и
# порчи: файл, изменённый после штамповки, обязан отставать, а изменённый ВМЕСТЕ с источником — нет.
docs_fixture_stamp() {
  local d="$1" ru
  shift
  for ru in "$@"; do
    DOCS_ROOT="$d" bash "${DOCS_FIXTURE_SELF:?каталог репозитория не задан}/scripts/docs_stamp.sh" "$ru" >/dev/null || return 1
  done
}
