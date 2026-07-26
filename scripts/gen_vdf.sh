#!/usr/bin/env bash
# Spec #8 Gate 5 — генерация SteamPipe VDF (app_build + per-OS depot).
# Один build → N депотов (депот на ОС). SetLive = beta-ветка (НЕ "default" —
# его нельзя авто-SetLive). appid/depotid из env; без секретов подставляются
# placeholder'ы 000000 → VDF валиден и парсится, но реальная заливка gated.
# Реальный upload делает steamcmd-ступень release.yml, только при наличии секретов.
set -euo pipefail

OUT="${1:?usage: gen_vdf.sh <out-dir> [version]}"
VERSION="${2:-${GAME_VERSION:-0.0.0-dev}}"
APPID="${STEAM_APPID:-000000}"
DEPOT_LINUX="${STEAM_DEPOT_LINUX:-000001}"
DEPOT_WINDOWS="${STEAM_DEPOT_WINDOWS:-000002}"
DEPOT_MACOS="${STEAM_DEPOT_MACOS:-000003}"
BRANCH="${STEAM_BRANCH:-beta}"   # никогда не "default" (авто-SetLive запрещён)
[ "$BRANCH" = "default" ] && { echo "refuse: SetLive=default запрещён" >&2; exit 1; }

# Санитизация (интерполируются в heredoc): VERSION — alnum/./-; appid/depot — только цифры.
case "$VERSION" in ''|*[!A-Za-z0-9.-]*) echo "refuse: невалидная version '$VERSION'" >&2; exit 1 ;; esac
for _id in "$APPID" "$DEPOT_LINUX" "$DEPOT_WINDOWS" "$DEPOT_MACOS"; do
  case "$_id" in ''|*[!0-9]*) echo "refuse: appid/depot '$_id' не числовой" >&2; exit 1 ;; esac
done

mkdir -p "$OUT"

depot_vdf() { # <depotid> <os-subdir> <file>
  cat > "$OUT/$3" <<EOF
"DepotBuild"
{
	"DepotID" "$1"
	"ContentRoot" "../content/$2"
	"FileMapping"
	{
		"LocalPath" "*"
		"DepotPath" "."
		"recursive" "1"
	}
}
EOF
}
depot_vdf "$DEPOT_LINUX"   linux   depot_linux.vdf
depot_vdf "$DEPOT_WINDOWS" windows depot_windows.vdf
depot_vdf "$DEPOT_MACOS"   macos   depot_macos.vdf

cat > "$OUT/app_build.vdf" <<EOF
"AppBuild"
{
	"AppID" "$APPID"
	"Desc" "like-nes $VERSION"
	"BuildOutput" "./output"
	"ContentRoot" "./content"
	"SetLive" "$BRANCH"
	"Depots"
	{
		"$DEPOT_LINUX" "depot_linux.vdf"
		"$DEPOT_WINDOWS" "depot_windows.vdf"
		"$DEPOT_MACOS" "depot_macos.vdf"
	}
}
EOF

echo "[gen_vdf] wrote app_build.vdf + depot_{linux,windows,macos}.vdf → $OUT"
echo "[gen_vdf] appid=$APPID branch=$BRANCH version=$VERSION (placeholder депоты: $([ "$APPID" = 000000 ] && echo yes || echo no))"
