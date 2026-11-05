# FNVSpeedhack

Управление **скоростью игры** в стиле Cheat Engine для Fallout: New Vegas,
упакованное так, чтобы просто работало везде: обычный **NVSE-плагин** для
Windows плюс **Decky-плагин** для Steam Deck. Переключение 1x / 2x / 3x / 5x
(и слоу-мо 0.5x) прямо во время игры.

[README in English](README.md)

## Как это работает

Ровно та же техника, что и в speedhack'е Cheat Engine: DLL хукает таймеры
Windows (`QueryPerformanceCounter`, `GetTickCount`, `GetTickCount64`,
`timeGetTime`) внутри процесса игры и умножает прошедшее время на выбранный
коэффициент. Игровые адреса памяти не используются, поэтому работает с
**любой версией FNV** (Steam, GOG, 1.4.0.725) и даже с другими играми на
Gamebryo (Fallout 3).

Примечания:

- Скорость игры = *всё*: передвижение, анимации, ИИ, меню. Звук остаётся
  нормальным (как и в Cheat Engine).
- Только одиночная игра. Сейвы не затрагиваются.

## Установка на Windows

1. Установите [NVSE](https://github.com/ellieseavey/xNVSE/releases) (xNVSE) —
   скопируйте его файлы в папку игры. Если уже стоит, пропустите.
2. Скопируйте `FNVSpeedhack.dll` в `Data\NVSE\Plugins\`.
3. Играйте. Хоткеи:
   - **F8** — цикл по пресетам: 1x → 2x → 3x → 5x → 1x
   - **F9 / F10** — шаг вверх / вниз
   - короткий бип подтверждает каждый пресет (1x низкий … 5x высокий)

Все хоткеи, пресеты и интервалы опроса настраиваются через
`FNVSpeedhack.ini` (пример: [`FNVSpeedhack.ini.example`](FNVSpeedhack.ini.example))
рядом с DLL. Для отладки рядом пишется `fnvspeed.log`.

Пользователи MO2: DLL лежит в реальной папке игры, поэтому загружается
независимо от MO2 — ничего настраивать не нужно.

## Установка на Steam Deck

Нужно: Decky Loader и работающий под Proton NVSE.

1. **NVSE под Proton** — скопируйте файлы xNVSE в папку игры
   (`.../steamapps/common/Fallout New Vegas/`) и задайте параметры запуска
   Steam (Свойства → Общие → Параметры запуска):

   ```
   WINEDLLOVERRIDES="nvse_steam_loader.dll=n,b" %command%
   ```

   (Стандартный способ загрузки xNVSE под Proton. Сам speedhack-DLL в
   override **не нуждается** — NVSE грузит его явно.)

2. **Decky-плагин** — скачайте `fnv-speed.zip` из Releases, распакуйте так,
   чтобы папка попала в `~/homebrew/plugins/fnv-speed/`, затем перезагрузите
   плагины (или перезапустите Steam).

3. В игровом режиме: `Быстрое меню (…) → Decky → FNV Speed`:
   - нажмите **Auto-detect** — плагин сам найдёт папку игры,
   - нажмите **Install NVSE plugin DLL** (встроенный DLL скопируется в
     `Data\NVSE\Plugins\`),
   - кнопки скорости: `0.5x 1x 1.5x 2x 3x 5x`.

Плагин общается с DLL через маленький файл в папке игры (`fnvspeed.txt`),
а DLL публикует текущую скорость обратно (`fnvspeed_status.txt`), поэтому
панель всегда показывает реальную скорость в игре.

**Хочется физических кнопок?** Плагин не обязателен: в Steam Input повесьте
`F8` (цикл) на бэк-падл или сделайте shift-слой — DLL на Deck слушает
клавиатурные хоткеи так же, как на Windows.

## Сборка из исходников

Windows / Linux — одинаково:

```sh
# DLL (нужен mingw-w64 i686, например winlibs; в Ubuntu: sudo apt install gcc-mingw-w64-i686)
./build.sh            # -> FNVSpeedhack.dll

# фронтенд Decky-плагина (нужен Node.js)
cd decky/fnv-speed
npm install
npm run build         # -> dist/index.js

# упаковка Decky-zip (сначала собрать DLL и dist)
cd ../..
./pack-decky.sh       # -> release/fnv-speed.zip
```

Есть автономный смоук-тест, который проверяет весь конвейер (хуки + файловый
IPC) без запуска игры:

```sh
gcc -O2 -o test_harness.exe test/test_harness.c -lkernel32 -luser32
./test_harness.exe test/run    # ожидает test/run/Data/NVSE/Plugins/FNVSpeedhack.dll
```

## Справочник настроек

| Ключ                 | По умолчанию   | Значение                                  |
|----------------------|----------------|-------------------------------------------|
| `HotkeyCycle`        | `119` (F8)     | цикл по пресетам, `0` = выкл (VK-код)     |
| `HotkeyUp`           | `120` (F9)     | шаг вверх, `0` = выкл                     |
| `HotkeyDown`         | `121` (F10)    | шаг вниз, `0` = выкл                      |
| `SpeedKeys`          | `97:1.0,98:3.0,99:5.0,100:10.0` | прямые бинды `vk:скорость` (Numpad1..4), до 8 |
| `Presets`            | `1.0,2.0,3.0,5.0,10.0` | до 16 значений, например `0.5` для слоу-мо |
| `StartupSpeed`       | `1.0`          | скорость при загрузке плагина             |
| `ControlFileEnabled` | `1`            | IPC fnvspeed.txt / fnvspeed_status.txt    |
| `HotkeyPollMs`       | `30`           | интервал опроса хоткеев                   |
| `ControlPollMs`      | `250`          | интервал опроса файла                     |
| `BeepFeedback`       | `1`            | бип при смене скорости                    |
| `LogEnabled`         | `1`            | писать fnvspeed.log                       |

### Рекомендуемая раскладка пэдлов на Steam Deck

В Steam Input (FNV → раскладка контроллера → Изменить → Задние грипп-кнопки):

| Пэдл | Клавиша | Скорость |
|--------|--------------|-------|
| L4 | Keypad 1 | 1x |
| L5 | Keypad 2 | 3x |
| R4 | Keypad 3 | 5x |
| R5 | Keypad 4 | 10x |

## Лицензия

MIT — см. [LICENSE](LICENSE). MinHook включён в состав под его собственной
лицензией MIT (`third_party/minhook/LICENSE.txt`).
