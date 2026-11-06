#!/usr/bin/env bash
# fnvspeed_setup.sh - one-command speedhack setup for Fallout: New Vegas.
#
# Finds your FNV install (PortProton prefixes, ~/games, Steam libraries),
# drops the dinput8 proxy + ini next to FalloutNV.exe, patches the Wine
# prefix (or Steam Proton prefix) with dinput8=native,builtin, and creates
# a launcher. After this the game runs accelerated: back paddles via the
# FNV Speed Decky plugin (L4=1x L5=3x R4=5x R5=10x) or F8/Numpad hotkeys.
#
# Usage: bash fnvspeed_setup.sh [--dry-run]
# Expects dinput8.dll (+ optional FNVSpeedhack.ini.example) next to itself;
# downloads them from the GitHub release if missing.

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROXY="$SCRIPT_DIR/dinput8.dll"
INI_SRC="$SCRIPT_DIR/FNVSpeedhack.ini.example"
INI_NAME="FNVSpeedhack.ini"
EXE="FalloutNV.exe"
REL_BASE="https://github.com/lightview/fnv-speedhack/releases/download/v0.3.1"
HOME_DIR="${HOME:-/home/deck}"

DRY=0
[[ "${1:-}" == "--dry-run" ]] && DRY=1

msg() { echo -e "\n==> $*"; }

# ---------------------------------------------------------------- locate game

CANDIDATES=()
scan_dir() {  # find FalloutNV.exe up to 5 levels deep, skip saves/xcache
    [[ -d "$1" ]] || return 0
    while IFS= read -r f; do
        CANDIDATES+=("${f%/$EXE}")
    done < <(find "$1" -maxdepth 5 -type f -name "$EXE" 2>/dev/null)
}

scan_dir "$HOME_DIR/PortProton/data/prefixes"
scan_dir "$HOME_DIR/games"
scan_dir "$HOME_DIR/.local/share/Steam/steamapps/common"
for lib in "$HOME_DIR"/.steam/steam/steamapps/common "$HOME_DIR"/.local/share/Steam/steamapps/common; do
    scan_dir "$lib"
done
# dedupe by real path (symlinked steam roots resolve the same) and sort:
# localized enplczru / repack-style copies first
mapfile -t CANDIDATES < <(printf '%s\n' "${CANDIDATES[@]}" | while read -r c; do realpath -m "$c"; done | awk '!seen[$0]++')
mapfile -t CANDIDATES < <(printf '%s\n' "${CANDIDATES[@]}" | awk '/enplczru/ {print; next} {rest[NR]=$0} END {for (i=1;i<=NR;i++) if (rest[i] !~ /enplczru/) print rest[i]}' )

if [[ ${#CANDIDATES[@]} -eq 0 ]]; then
    msg "FalloutNV.exe не найден. Установи игру (например, через PortProton) и запусти скрипт снова."
    exit 1
fi

GAME=""
if [[ ${#CANDIDATES[@]} -eq 1 ]] || [[ ! -t 0 ]]; then
    GAME="${CANDIDATES[0]}"
    [[ ${#CANDIDATES[@]} -gt 1 ]] && msg "Копий несколько - беру первую: $GAME (запусти в терминале для выбора)"
else
    echo "Найдено несколько копий New Vegas:"
    i=1
    for c in "${CANDIDATES[@]}"; do echo "  [$i] $c"; i=$((i+1)); done
    read -rp "Какую настраивать? (номер): " pick
    [[ "$pick" =~ ^[0-9]+$ ]] && [[ "$pick" -ge 1 ]] && [[ "$pick" -le ${#CANDIDATES[@]} ]] || { echo "Неверный номер"; exit 1; }
    GAME="${CANDIDATES[$((pick-1))]}"
fi
msg "Игра: $GAME"

# ---------------------------------------------------------------- proxy files

if [[ ! -f "$PROXY" ]]; then
    msg "dinput8.dll рядом со скриптом нет - качаю с GitHub..."
    curl -fL --retry 2 -o "$PROXY" "$REL_BASE/dinput8.dll" || { echo "скачать не удалось, положи dinput8.dll рядом со скриптом"; exit 1; }
fi
[[ -f "$INI_SRC" ]] || INI_SRC="$PROXY"  # no ini - defaults are fine

if [[ $DRY -eq 1 ]]; then
    msg "DRY-RUN: скопировал бы $PROXY и $INI_NAME в $GAME/"
else
    cp -f "$PROXY" "$GAME/dinput8.dll"
    cp -f "$INI_SRC" "$GAME/$INI_NAME"
    msg "Установлено: $GAME/dinput8.dll + $INI_NAME"
fi

# ---------------------------------------------------------------- wine prefix override

patch_user_reg() {  # $1 = path to user.reg
    local reg="$1"
    [[ -f "$reg" ]] || return 1
    grep -q '"dinput8"="native' "$reg" 2>/dev/null && { echo "    override уже стоит: $reg"; return 0; }
    if [[ $DRY -eq 1 ]]; then
        echo "    (dry-run) дописал бы dinput8=native,builtin в $reg"
        return 0
    fi
    cp -f "$reg" "$reg.bak-fnvspeed" 2>/dev/null
    { echo ""
      echo '[Software\\Wine\\DllOverrides] '"$(date +%s)"
      echo '"dinput8"="native,builtin"'
    } >> "$reg"
    echo "    override добавлен: $reg"
    return 0
}

case "$GAME" in
    */drive_c/*)
        PREFIX_ROOT="${GAME%%/drive_c/*}"
        msg "PortProton-префикс: $PREFIX_ROOT"
        patch_user_reg "$PREFIX_ROOT/user.reg"
        ;;
    */steamapps/common/*)
        msg "Steam-копия: ищу её Proton-префикс (compatdata)..."
        STEAMAPPS="${GAME%%/steamapps/common/*}/steamapps"
        FOUND_APPID=""
        for acf in "$STEAMAPPS"/appmanifest_*.acf; do
            [[ -f "$acf" ]] || continue
            installdir=$(grep -aoE '"installdir"\s+"[^"]+"' "$acf" | head -1 | sed 's/.*"\([^"]*\)"/\1/')
            [[ "$GAME" == */steamapps/common/"$installdir" ]] && FOUND_APPID="$(basename "$acf" | sed 's/appmanifest_//; s/.acf//')" && break
        done
        if [[ -n "$FOUND_APPID" ]]; then
            patch_user_reg "$STEAMAPPS/compatdata/$FOUND_APPID/pfx/user.reg"
        else
            echo "    appid не найден - добавь в параметрах запуска Steam:"
            echo '    WINEDLLOVERRIDES="dinput8=n,b" %command%'
        fi
        ;;
    *)
        msg "Нестандартный путь - добавь override вручную (winecfg: dinput8 = native,builtin)"
        ;;
esac

# ---------------------------------------------------------------- steam shortcut

if [[ "$GAME" == "$HOME_DIR/PortProton/data/prefixes/"*"/drive_c/"* ]]; then
    if [[ $DRY -eq 1 ]]; then
        msg "DRY-RUN: создал бы ярлык в Steam через add_in_steam.sh"
    elif [[ -f "$HOME_DIR/PortProton/data/scripts/add_in_steam.sh" ]]; then
        msg "Создаю ярлык в Steam..."
        APPNAME="Fallout New Vegas (Speedhack)"
        bash -c "
            export PORT_SCRIPTS_PATH='$HOME_DIR/PortProton/data/scripts'
            export PORT_WINE_PATH='$HOME_DIR/PortProton'
            source '$HOME_DIR/PortProton/data/scripts/add_in_steam.sh'
            NOSTAPPNAME='$APPNAME'
            NOSTEXEPATH='$GAME/$EXE'
            NOSTAPPID=''
            addEntry
        " 2>/dev/null && msg "Ярлык '$APPNAME' добавлен в Steam (перезапусти Steam если не виден)" \
                      || msg "Ярлык создать не удалось - добавь $GAME/$EXE в Steam вручную"
    fi
else
    msg "Добавь $GAME/$EXE в Steam вручную: Библиотека -> Добавить игру -> Обзор"
fi

# ---------------------------------------------------------------- done

msg "ГОТОВО! Скорость переключается:"
echo "  - Steam Deck + Decky-плагин 'FNV Speed': пэдлы L4=1x L5=3x R4=5x R5=10x"
echo "  - Windows/клавиатура: F8 цикл, F9/F10 шаг, Numpad 1-4 напрямую"
echo "  - Настройки: $GAME/$INI_NAME"
[[ $DRY -eq 1 ]] && msg "это был dry-run - ничего не изменено"
