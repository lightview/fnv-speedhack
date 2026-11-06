# FNVSpeedhack

Управление **скоростью игры** в стиле Cheat Engine для Fallout: New Vegas —
от 1x до 10x (и слоу-мо) по нажатию кнопки. Поставляется как крошечный DLL
плюс опциональный **Decky-плагин** для Steam Deck: переключение скорости —
задние пэдлы **L4 = 1x, L5 = 3x, R4 = 5x, R5 = 10x**, без Steam Input
биндингов, без модов, без Cheat Engine.

[README in English](README.md)

## Как это работает

Та же техника, что у speedhack'а Cheat Engine, но своя реализация: DLL хукает
таймеры Windows внутри процесса игры (`QueryPerformanceCounter`,
`GetTickCount(64)`, `timeGetTime` через [MinHook](https://github.com/TsudaKageyu/minhook))
и умножает прошедшее время на коэффициент. Никаких игровых адресов и патчей
exe — работает с **любой** сборкой FNV (Steam, GOG, любой язык) и даже с
другими играми на Gamebryo (Fallout 3).

Ускоряется всё: движение, ИИ, анимации, меню. Звук остаётся обычным (как и в
Cheat Engine). Только одиночная игра, сейвы не затрагиваются.

Две взаимозаменяемые сборки из одной кодовой базы:

| Сборка | Файл | Куда класть | Требует |
|---|---|---|---|
| NVSE-плагин | `FNVSpeedhack.dll` | `Data\NVSE\Plugins\` | xNVSE |
| **dinput8-прокси** | `dinput8.dll` | корень игры | ничего (любой exe) |

Прокси — рекомендуемый путь везде: `FalloutNV.exe` импортирует `dinput8.dll`
и грузит его сам. На Windows просто положи файл в папку игры. Под Proton
добавь `WINEDLLOVERRIDES="dinput8=n,b" %command%` (кнопка установки
Decky-плагина делает это за тебя через реестр префикса).

## Установка на Steam Deck

1. Установи Decky-плагин из `fnv-speed.zip` (Releases) — распакуй в
   `~/homebrew/plugins/fnv-speed/` и перезапусти Decky (или поставь zip через
   ручную установку в магазине Decky).
2. QAM (…) → Decky → **FNV Speed** → **Install speedhack**.
   Одна кнопка: копирует прокси-DLL в папку игры, пишет ini и ставит
   `dinput8=native,builtin` в нужный Proton-префикс (appid находится по
   appmanifest, локализованные депоты включены).
3. Играй. Пэдлы переключают скорость: **L4 = 1x, L5 = 3x, R4 = 5x,
   R5 = 10x** — читаются прямо из контроллера через hidraw, поэтому Steam
   Input не нужен вообще. В QAM-панели тоже есть сетка пресетов 3×2,
   слайдер точной настройки и тосты при каждой смене скорости.

## Установка на Windows

1. Возьми `dinput8.dll` из релиза и положи в корень игры (рядом с
   `FalloutNV.exe`). Всё.
   (Или NVSE-вариант в `Data\NVSE\Plugins\`, если у тебя xNVSE.)
2. Хоткеи: **F8** цикл, **F9/F10** шаг, **Numpad 1–4** напрямую
   1x/3x/5x/10x. Короткий бип подтверждает смену.
3. Всё настраивается в `FNVSpeedhack.ini` рядом с DLL — см. справочник ниже.
   Рядом пишется `fnvspeed.log`.

## Справочник настроек

| Ключ | По умолчанию | Значение |
|---|---|---|
| `HotkeyCycle` | `119` (F8) | цикл по пресетам, `0` = выкл (VK-код) |
| `HotkeyUp` / `HotkeyDown` | `120` / `121` | шаг вверх / вниз, `0` = выкл |
| `SpeedKeys` | `97:1.0,98:3.0,99:5.0,100:10.0` | прямые бинды `vk:скорость` (Numpad1–4) |
| `PadModifier` | *(пусто)* | XInput-комбо: напр. `LB+RB`, скорости в `PadA/B/X/Y`; на Деке выключено |
| `Presets` | `1.0,2.0,3.0,5.0,10.0` | список пресетов для цикла/шага (до 16) |
| `StartupSpeed` | `1.0` | скорость при загрузке |
| `ControlFileEnabled` | `1` | IPC `fnvspeed.txt` / `fnvspeed_status.txt` |
| `ControlPollMs` / `HotkeyPollMs` | `250` / `30` | интервалы опроса |
| `BeepFeedback` | `1` | бип при смене скорости |
| `LogEnabled` | `1` | писать `fnvspeed.log` |

Файловый IPC: запиши число (`3.0`) или `toggle` / `up` / `down` / `reset` в
`fnvspeed.txt` в корне игры — DLL применит за ~250 мс и сообщит текущую
скорость в `fnvspeed_status.txt`. Это то, чем пользуется Decky-панель — и можно
дёргать откуда угодно (включая SSH).

## Сборка из исходников

```sh
# DLL (mingw-w64 i686, например winlibs; Ubuntu: sudo apt install gcc-mingw-w64-i686)
./build.sh            # -> FNVSpeedhack.dll + dist/dinput8.dll

# фронтенд Decky-плагина (Node.js 20+)
cd decky/fnv-speed && npm install && npm run build

# упаковка decky-zip
cd ../.. && ./pack-decky.sh   # -> release/fnv-speed.zip
```

Автономный смоук-тест проверяет хуки, хоткеи и файловый IPC без игры:

```sh
gcc -O2 -o test_harness.exe test/test_harness.c -lkernel32 -luser32
./test_harness.exe test/run              # NVSE-сборка
./test_harness.exe test/run_proxy proxy  # dinput8-прокси
```

## Авторы и лицензия

MIT — см. [LICENSE](LICENSE). MinHook включён под собственной лицензией MIT.
Чтение пэдлов — по раскладке отчётов `hid-steam.c` из ядра Linux.
